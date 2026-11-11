#pragma once

#include <cstdint>
#include <vector>

#include "espnow_codec.h"
#include "group_sync.h"

// Thin ESP-NOW transport: sends and receives codec messages, nothing more.
class EspNowLink {
 public:
  void begin();
  // Replaces the encrypted Member peers with every member except self.
  void set_members(const std::vector<sls::Mac>& members, const sls::Mac& self);
  void send_hello(uint32_t group_id, const sls::Mac& self, const sls::PairingInfo& info);
  void send_ack(const sls::Mac& to, uint32_t group_id, const sls::Mac& self, const sls::PairingInfo& info);
  // Unicasts each message to its addressee, an encrypted Member peer.
  void send_all(const std::vector<sls::Outgoing>& outgoing);
  // Moves the radio to a Wi-Fi channel (1-14); does nothing when already on it.
  void set_channel(uint8_t channel);
  // Pops received frames until one decodes cleanly; false once the queue is empty.
  // `from` is the frame's radio source MAC, which the sender field inside the message is not.
  bool poll(sls::Message& out, sls::Mac& from, uint32_t my_group_id);
  sls::Mac self_mac() const;

 private:
  void send(const sls::Mac& to, const sls::Message& message);
  void remove_known_peers();
  bool add_peer(const sls::Mac& mac, bool encrypted);

  std::vector<sls::Mac> peers_;
};
