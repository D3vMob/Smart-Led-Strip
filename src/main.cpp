#include <Arduino.h>

#include <memory>
#include <variant>

#include "blink.h"
#include "button_gesture.h"
#include "config.h"
#include "espnow_link.h"
#include "group_state.h"
#include "group_store.h"
#include "ld2410_uart.h"
#include "matter_endpoints.h"
#include "pairing.h"
#include "save_scheduler.h"
#include "state_store.h"
#include "strip_output.h"

namespace {

using SensorStatus = sls::ld2410::SensorDetection::Status;

StripOutput strip(LED_COUNT, LED_DATA_PIN, LED_PIXEL_TYPE, MAX_MILLIAMPS);
Ld2410Uart sensor(Serial1, LD2410_RX_PIN, LD2410_TX_PIN, LD2410_DETECT_TIMEOUT_MS);
sls::GroupState group_state = sls::default_group_state();
sls::SaveScheduler scheduler(group_state, PERSIST_SETTLE_MS);
sls::BlinkClock blink_clock;
sls::ButtonGesture button;
EspNowLink espnow;
sls::Mac self_mac = {};
std::unique_ptr<sls::Pairing> pairing;
SensorStatus reported_status = SensorStatus::Probing;
bool reported_present = false;

void report_sensor_status(uint32_t now_ms) {
  SensorStatus status = sensor.status(now_ms);
  if (status == reported_status) return;
  reported_status = status;
  Serial.println(status == SensorStatus::Sensor ? "LD2410C detected: Sensor unit"
                                                : "No LD2410C detected");
}

void report_presence(const sls::ld2410::Presence& presence) {
  if (presence.onset) {
    Serial.printf("Presence onset (moving %u cm/%u, still %u cm/%u)\n",
                  presence.report.moving_distance_cm, presence.report.moving_energy,
                  presence.report.still_distance_cm, presence.report.still_energy);
  } else if (reported_present && !presence.present) {
    Serial.println("Presence ended");
  }
  reported_present = presence.present;
}

// Breathing cyan shown while the pairing window is open: a 1 s triangle wave, 10-60 % brightness.
sls::Look pairing_look(uint32_t now_ms) {
  const uint32_t phase = now_ms % 1000;
  const uint32_t rise = phase < 500 ? phase : 1000 - phase;  // 0..500
  sls::Look look = group_state.look;
  look.color_mode = sls::ColorMode::HueSat;
  look.hue_deg = 180;
  look.saturation = 255;  // full saturation (0-255 scale)
  look.brightness_pct = static_cast<uint8_t>(10 + rise * 50 / 500);
  look.blink_on = false;
  return look;
}

void apply_pairing_actions(const sls::PairingActions& actions, uint32_t now_ms) {
  if (actions.send_hello) espnow.send_hello(pairing->group_id(), self_mac, pairing->pairing_info(now_ms));
  if (actions.send_ack_to) {
    espnow.send_ack(*actions.send_ack_to, pairing->group_id(), self_mac, pairing->pairing_info(now_ms));
  }
  if (actions.group_formed) {
    save_group_members(pairing->members());
    clear_motion_master_keys();
    espnow.set_members(pairing->members(), self_mac);
    Serial.printf("Group formed: id %08x, %u member(s)\n",
                  static_cast<unsigned>(pairing->group_id()),
                  static_cast<unsigned>(pairing->members().size()));
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  strip.begin();
  sensor.begin(millis());
  // The Strip comes back the way it was left; a fresh Unit starts in the defaults (Closed).
  group_state = load_group_state();
  scheduler = sls::SaveScheduler(group_state, PERSIST_SETTLE_MS);
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP);
  espnow.begin();
  self_mac = espnow.self_mac();
  pairing = std::make_unique<sls::Pairing>(self_mac, load_group_members());
  espnow.set_members(pairing->members(), self_mac);
  matter_endpoints::begin(group_state);
}

void loop() {
  const uint32_t now_ms = millis();
  sls::ld2410::Presence presence;
  if (sensor.poll(now_ms, presence)) report_presence(presence);
  report_sensor_status(now_ms);
  if (button.update(digitalRead(BOOT_BUTTON_PIN) == LOW, now_ms) == sls::Gesture::LongPress) {
    apply_pairing_actions(pairing->on_long_press(now_ms), now_ms);
  }
  sls::Message message;
  while (espnow.poll(message, pairing->group_id())) {
    const sls::PairingInfo* info = std::get_if<sls::PairingInfo>(&message.body);
    if (info == nullptr) continue;
    if (message.type == sls::MessageType::PairingHello) {
      apply_pairing_actions(pairing->on_hello(message.sender, *info, now_ms), now_ms);
    } else if (message.type == sls::MessageType::PairingAck) {
      apply_pairing_actions(pairing->on_ack(message.sender, *info, now_ms), now_ms);
    }
  }
  apply_pairing_actions(pairing->on_tick(now_ms), now_ms);
  // Apply voice writes, then report back the real state, including clamped
  // values and changes made here (motion, boot restore).
  matter_endpoints::sync(group_state);
  scheduler.on_change(group_state, now_ms);
  if (scheduler.due(now_ms)) {
    save_group_state(group_state);
  }
  const uint32_t phase = blink_clock.phase_ms(now_ms, sls::blink_period_ms(group_state.look.blink_speed_pct));
  const bool lit = sls::blink_lit(group_state.look, group_state.open, phase);
  if (pairing->window_open()) {
    strip.render(pairing_look(now_ms), true);
  } else {
    strip.render(group_state.look, lit);
  }
  // Short enough that the 50 ms minimum blink period still shows (25 ms on, 25 ms off).
  delay(10);
}
