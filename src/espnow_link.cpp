#include "espnow_link.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <atomic>
#include <cstring>

#include "config.h"

namespace {

static_assert(sizeof(ESPNOW_LMK) - 1 == ESP_NOW_KEY_LEN, "ESPNOW_LMK must be exactly 16 characters");

constexpr size_t kRingSlots = 8;
constexpr sls::Mac kBroadcast = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct Frame {
  uint8_t data[sls::kMaxMessageBytes];
  uint8_t length;
  sls::Mac from;
};

// Single producer (Wi-Fi task) and single consumer (loop); a full ring drops new frames.
Frame ring[kRingSlots];
volatile uint8_t ring_head = 0;
volatile uint8_t ring_tail = 0;

void on_receive(const esp_now_recv_info_t* recv_info, const uint8_t* data, int length) {
  if (recv_info == nullptr || recv_info->src_addr == nullptr) return;
  if (length <= 0 || length > static_cast<int>(sls::kMaxMessageBytes)) return;
  const uint8_t next = (ring_head + 1) % kRingSlots;
  if (next == ring_tail) return;
  std::memcpy(ring[ring_head].data, data, length);
  ring[ring_head].length = static_cast<uint8_t>(length);
  std::memcpy(ring[ring_head].from.data(), recv_info->src_addr, ring[ring_head].from.size());
  ring_head = next;
}

// Sends handed to the radio and not yet confirmed; raised by the loop, lowered by the Wi-Fi send callback.
std::atomic<uint8_t> pending_sends{0};

void on_sent(const esp_now_send_info_t*, esp_now_send_status_t) {
  uint8_t pending = pending_sends.load();
  while (pending > 0 && !pending_sends.compare_exchange_weak(pending, pending - 1)) {
  }
}

}  // namespace

void EspNowLink::begin() {
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(ESPNOW_PAIRING_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  esp_now_peer_info_t broadcast = {};
  std::memcpy(broadcast.peer_addr, kBroadcast.data(), kBroadcast.size());
  broadcast.channel = 0;
  broadcast.ifidx = WIFI_IF_STA;
  broadcast.encrypt = false;
  esp_now_add_peer(&broadcast);
  esp_now_register_recv_cb(on_receive);
  esp_now_register_send_cb(on_sent);
}

bool EspNowLink::add_peer(const sls::Mac& mac, bool encrypted) {
  esp_now_peer_info_t peer = {};
  std::memcpy(peer.peer_addr, mac.data(), mac.size());
  peer.channel = 0;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = encrypted;
  if (encrypted) std::memcpy(peer.lmk, ESPNOW_LMK, ESP_NOW_KEY_LEN);
  if (esp_now_add_peer(&peer) != ESP_OK) return false;
  peers_.push_back(mac);
  return true;
}

void EspNowLink::remove_known_peers() {
  for (const sls::Mac& mac : peers_) esp_now_del_peer(mac.data());
  peers_.clear();
}

void EspNowLink::set_members(const std::vector<sls::Mac>& members, const sls::Mac& self) {
  remove_known_peers();
  for (const sls::Mac& mac : members) {
    if (mac != self) add_peer(mac, true);
  }
}

void EspNowLink::send(const sls::Mac& to, const sls::Message& message) {
  uint8_t buffer[sls::kMaxMessageBytes];
  const size_t length = sls::encode(message, buffer, sizeof(buffer));
  if (length == 0) return;
  if (esp_now_send(to.data(), buffer, length) == ESP_OK) pending_sends.fetch_add(1);
}

void EspNowLink::send_all(const std::vector<sls::Outgoing>& outgoing) {
  for (const sls::Outgoing& item : outgoing) send(item.to, item.message);
}

void EspNowLink::set_channel(uint8_t channel) {
  if (channel < 1 || channel > 14) return;
  uint8_t current = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  if (esp_wifi_get_channel(&current, &second) == ESP_OK && current == channel) return;
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

void EspNowLink::send_hello(uint32_t group_id, const sls::Mac& self, const sls::PairingInfo& info) {
  send(kBroadcast, sls::Message{sls::MessageType::PairingHello, group_id, self, info});
}

void EspNowLink::send_ack(const sls::Mac& to, uint32_t group_id, const sls::Mac& self,
                          const sls::PairingInfo& info) {
  if (!esp_now_is_peer_exist(to.data())) add_peer(to, false);
  send(to, sls::Message{sls::MessageType::PairingAck, group_id, self, info});
}

bool EspNowLink::poll(sls::Message& out, sls::Mac& from, uint32_t my_group_id) {
  // Skips frames that don't decode (e.g. another Group's) so they never stall the ones behind.
  while (ring_tail != ring_head) {
    const Frame& frame = ring[ring_tail];
    const sls::Decoded decoded = sls::decode(frame.data, frame.length, my_group_id);
    ring_tail = (ring_tail + 1) % kRingSlots;
    if (decoded.ok) {
      out = decoded.message;
      from = frame.from;
      return true;
    }
  }
  return false;
}

sls::Mac EspNowLink::self_mac() const {
  sls::Mac mac = {};
  esp_read_mac(mac.data(), ESP_MAC_WIFI_STA);
  return mac;
}

void EspNowLink::flush(uint32_t timeout_ms) {
  const uint32_t start = millis();
  while (pending_sends > 0 && millis() - start < timeout_ms) delay(1);
}
