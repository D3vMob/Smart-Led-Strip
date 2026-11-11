#include <Arduino.h>
#include <WiFi.h>

#include <memory>
#include <optional>
#include <variant>

#include "blink.h"
#include "button_gesture.h"
#include "config.h"
#include "espnow_link.h"
#include "group_state.h"
#include "group_store.h"
#include "group_sync.h"
#include "ld2410_uart.h"
#include "matter_endpoints.h"
#include "motion_master.h"
#include "motion_rules.h"
#include "pairing.h"
#include "save_scheduler.h"
#include "state_store.h"
#include "strip_output.h"

namespace {

using SensorStatus = sls::ld2410::SensorDetection::Status;

// A Sensor unit repeats its presence level this often even when it has not changed.
constexpr uint32_t kPresenceKeepaliveMs = 2000;
// The confirmation after a double-press: two white flashes, 150 ms on and 150 ms off.
constexpr uint32_t kFlashHalfPeriodMs = 150;
constexpr uint32_t kFlashTotalMs = 600;

StripOutput strip(LED_COUNT, LED_DATA_PIN, LED_PIXEL_TYPE, MAX_MILLIAMPS);
Ld2410Uart sensor(Serial1, LD2410_RX_PIN, LD2410_TX_PIN, LD2410_DETECT_TIMEOUT_MS);
sls::GroupState group_state = sls::default_group_state();
sls::SaveScheduler scheduler(group_state, PERSIST_SETTLE_MS);
sls::ButtonGesture button;
EspNowLink espnow;
sls::Mac self_mac = {};
std::unique_ptr<sls::Pairing> pairing;
std::unique_ptr<sls::GroupSync> group_sync;
bool matter_started = false;
uint32_t boot_count = 0;
sls::SyncRecord saved_record;
SensorStatus reported_status = SensorStatus::Probing;
bool reported_present = false;
// Sensor unit side: the current presence level and the time spent present.
bool sensor_present = false;
uint32_t total_presence_ms = 0;
uint32_t last_sensor_tick_ms = 0;
std::optional<bool> sent_level;
uint32_t last_presence_sent_ms = 0;
bool flashing = false;
uint32_t flash_start_ms = 0;
// Election side, on the Unit that owns the Group state.
std::unique_ptr<sls::MotionMaster> motion_master;
bool motion_master_loaded = false;  // built from flash (Coordinator) rather than fresh in RAM
sls::MotionMasterRecord saved_motion_record;
std::optional<sls::Mac> reported_master;
std::optional<bool> reported_motion_presence;
sls::MotionRules motion_rules;
bool rules_motion_enabled = false;

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
  sensor_present = presence.present;
}

// Whether this Unit runs the Motion master election: it owns the Group state.
bool runs_election() { return group_sync->matter_enabled(); }

bool same_scores(const sls::MotionMasterRecord& a, const sls::MotionMasterRecord& b) {
  if (a.override_mac != b.override_mac || a.onsets_counted != b.onsets_counted || a.scores.size() != b.scores.size()) {
    return false;
  }
  for (size_t i = 0; i < a.scores.size(); ++i) {
    if (a.scores[i].mac != b.scores[i].mac || a.scores[i].points != b.scores[i].points) return false;
  }
  return true;
}

// Builds the election for this role: saved in flash on a Coordinator, fresh in RAM on an Undecided
// Unit, none on a Member.
void ensure_motion_master() {
  if (!runs_election()) {
    motion_master.reset();
    motion_master_loaded = false;
    return;
  }
  const bool coordinator = group_sync->role() == sls::SyncRole::Coordinator;
  if (motion_master && (motion_master_loaded || !coordinator)) return;
  const std::optional<sls::MotionMasterRecord> saved = coordinator ? load_motion_master_record() : std::nullopt;
  saved_motion_record = saved ? *saved : sls::MotionMasterRecord{};
  motion_master = std::make_unique<sls::MotionMaster>(saved_motion_record);
  motion_master_loaded = coordinator;
  reported_master.reset();
  reported_motion_presence.reset();
}

// Saves the Coordinator's record when more than the running presence totals changed.
void save_motion_master_if_changed() {
  if (!motion_master || !motion_master_loaded) return;
  const sls::MotionMasterRecord record = motion_master->record();
  if (same_scores(record, saved_motion_record)) return;
  save_motion_master_record(record);
  saved_motion_record = record;
}

void report_master_change() {
  const std::optional<sls::Mac> master = motion_master->master();
  if (master == reported_master) return;
  reported_master = master;
  if (!master) {
    Serial.println("Motion master: none");
    return;
  }
  Serial.printf("Motion master: %02x:%02x:%02x:%02x:%02x:%02x\n", (*master)[0], (*master)[1], (*master)[2],
                (*master)[3], (*master)[4], (*master)[5]);
}

void apply_motion_intent(sls::MotionIntent intent) {
  if (intent == sls::MotionIntent::Open) group_state = sls::apply(group_state, sls::Command::open());
  if (intent == sls::MotionIntent::Close) group_state = sls::apply(group_state, sls::Command::close());
}

// Motion rules follow the master's presence and open or close the Strip.
void run_motion_rules(uint32_t now_ms) {
  if (!motion_master) return;
  motion_master->on_tick(now_ms);
  report_master_change();
  if (group_state.motion_enabled != rules_motion_enabled) {
    rules_motion_enabled = group_state.motion_enabled;
    motion_rules.on_motion_enabled(rules_motion_enabled, now_ms);
  }
  const std::optional<bool> motion_presence = motion_master->motion_presence();
  if (motion_presence && motion_presence != reported_motion_presence) {
    apply_motion_intent(motion_rules.on_presence(*motion_presence, now_ms));
  }
  reported_motion_presence = motion_presence;
  apply_motion_intent(motion_rules.on_tick(now_ms));
  save_motion_master_if_changed();
}

bool same_look(const sls::Look& a, const sls::Look& b) {
  return a.color_mode == b.color_mode && a.hue_deg == b.hue_deg && a.saturation == b.saturation &&
         a.color_temp_mireds == b.color_temp_mireds && a.brightness_pct == b.brightness_pct &&
         a.blink_on == b.blink_on && a.blink_speed_pct == b.blink_speed_pct;
}

// Sends this Sensor unit's presence level to whoever runs the election.
void send_presence_report(uint32_t now_ms) {
  if (runs_election()) {
    if (motion_master) motion_master->on_presence_report(self_mac, sensor_present, total_presence_ms, now_ms);
  } else {
    espnow.send_all(group_sync->report_presence(sensor_present, total_presence_ms));
  }
  sent_level = sensor_present;
  last_presence_sent_ms = now_ms;
}

// A Sensor unit reports its presence on every change and as a keepalive.
void report_presence_level(uint32_t now_ms) {
  if (sensor.status(now_ms) != SensorStatus::Sensor) return;
  if (sensor_present) total_presence_ms += now_ms - last_sensor_tick_ms;
  last_sensor_tick_ms = now_ms;
  const bool changed = !sent_level || *sent_level != sensor_present;
  if (changed || now_ms - last_presence_sent_ms >= kPresenceKeepaliveMs) send_presence_report(now_ms);
}

// A double-press on a Sensor unit claims the Motion master role.
void handle_double_press(uint32_t now_ms) {
  if (sensor.status(now_ms) != SensorStatus::Sensor) return;
  if (runs_election()) {
    if (!motion_master) return;
    motion_master->on_override(self_mac, now_ms);
  } else {
    const std::vector<sls::Outgoing> claim = group_sync->claim_override();
    if (claim.empty()) return;  // no Coordinator to claim from yet: no confirmation
    espnow.send_all(claim);
  }
  flashing = true;
  flash_start_ms = now_ms;
  Serial.println("Override claimed: this Sensor unit asks to be the Motion master");
}

// Two short white flashes confirm the double-press, shown instead of the normal frame.
bool render_confirmation(uint32_t now_ms) {
  if (!flashing) return false;
  const uint32_t elapsed = now_ms - flash_start_ms;
  if (elapsed >= kFlashTotalMs) {
    flashing = false;
    return false;
  }
  sls::Look look = group_state.look;
  look.color_mode = sls::ColorMode::HueSat;
  look.hue_deg = 0;
  look.saturation = 0;
  look.brightness_pct = 100;
  look.blink_on = false;
  strip.render(look, (elapsed / kFlashHalfPeriodMs) % 2 == 0);
  return true;
}

// A Presence or OverrideClaim from a Sensor unit, for the Unit that runs the election.
void handle_member_report(const sls::Message& message, const sls::Mac& from, uint32_t now_ms) {
  if (!motion_master || !group_sync->on_member_report(message, from)) return;
  if (const sls::Presence* report = std::get_if<sls::Presence>(&message.body)) {
    motion_master->on_presence_report(message.sender, report->present, report->total_presence_ms, now_ms);
  } else if (message.type == sls::MessageType::OverrideClaim) {
    motion_master->on_override(message.sender, now_ms);
  }
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

void build_group_sync() {
  saved_record = load_sync_record();
  group_sync = std::make_unique<sls::GroupSync>(self_mac, pairing->group_id(), pairing->members(),
                                                pairing->group_secret(), saved_record, boot_count, group_state);
}

// Saves the role, Coordinator and Coordinator boot when they differ from what is in flash.
void save_record_if_changed() {
  const sls::SyncRecord record = group_sync->record();
  if (record.role == saved_record.role && record.coordinator == saved_record.coordinator &&
      record.coordinator_boot == saved_record.coordinator_boot) {
    return;
  }
  save_sync_record(record);
  saved_record = record;
}

void handle_sync_message(const sls::Message& message, const sls::Mac& from, uint32_t now_ms) {
  const sls::SyncRole before = group_sync->role();
  const bool applied = group_sync->on_message(message, from, now_ms);
  const sls::StateSync* sync = std::get_if<sls::StateSync>(&message.body);
  // The channel comes off the air, so never trust it beyond the channels hunted.
  if (applied && message.type == sls::MessageType::Heartbeat && sync != nullptr &&
      group_sync->role() == sls::SyncRole::Member && sync->wifi_channel >= 1 &&
      sync->wifi_channel <= sls::kChannelHuntLast) {
    espnow.set_channel(sync->wifi_channel);
  }
  save_record_if_changed();
  if (before != sls::SyncRole::Member && group_sync->role() == sls::SyncRole::Member) {
    if (matter_started) {
      Serial.println("Another Unit is the Coordinator: restarting to stay Matter-silent");
      ESP.restart();
    }
  }
}

void apply_pairing_actions(const sls::PairingActions& actions, uint32_t now_ms) {
  if (actions.send_hello) espnow.send_hello(pairing->group_id(), self_mac, pairing->pairing_info(now_ms));
  if (actions.send_ack_to) {
    espnow.send_ack(*actions.send_ack_to, pairing->group_id(), self_mac, pairing->pairing_info(now_ms));
  }
  if (actions.group_formed) {
    save_group_members(pairing->members());
    save_group_secret(pairing->group_secret());
    clear_motion_master_keys();
    motion_master.reset();  // rebuilt fresh by ensure_motion_master
    motion_master_loaded = false;
    espnow.set_members(pairing->members(), self_mac);
    group_sync->regroup(pairing->group_id(), pairing->members(), pairing->group_secret());
    save_record_if_changed();
    Serial.printf("Group formed: id %08x, %u member(s)\n",
                  static_cast<unsigned>(pairing->group_id()),
                  static_cast<unsigned>(pairing->members().size()));
    if (!matter_started) {
      Serial.println("Restarting so this Unit boots Matter-capable for the new Group");
      ESP.restart();
    }
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
  boot_count = next_boot_count();
  const std::optional<sls::GroupSecret> secret = load_group_secret();
  if (secret) {
    pairing = std::make_unique<sls::Pairing>(self_mac, load_group_members(), *secret);
  } else {
    pairing = std::make_unique<sls::Pairing>(self_mac);
    Serial.println("No Group secret saved: re-pair the Units to authenticate their sync");
  }
  espnow.set_members(pairing->members(), self_mac);
  build_group_sync();
  if (group_sync->matter_enabled()) {
    matter_endpoints::begin(group_state);
    matter_started = true;
  } else {
    Serial.println("Member: Matter-silent, following the Coordinator");
  }
}

void loop() {
  const uint32_t now_ms = millis();
  sls::ld2410::Presence presence;
  if (sensor.poll(now_ms, presence)) report_presence(presence);
  report_sensor_status(now_ms);
  ensure_motion_master();
  const sls::Gesture gesture = button.update(digitalRead(BOOT_BUTTON_PIN) == LOW, now_ms);
  if (gesture == sls::Gesture::LongPress) {
    sls::PairingNonce nonce;
    esp_fill_random(nonce.data(), nonce.size());
    apply_pairing_actions(pairing->on_long_press(now_ms, nonce), now_ms);
  } else if (gesture == sls::Gesture::DoublePress) {
    handle_double_press(now_ms);
  }
  report_presence_level(now_ms);
  sls::Message message;
  sls::Mac from;
  while (espnow.poll(message, from, pairing->group_id())) {
    if (message.type == sls::MessageType::Presence || message.type == sls::MessageType::OverrideClaim) {
      handle_member_report(message, from, now_ms);
      continue;
    }
    if (sls::is_sync_message(message.type)) {
      handle_sync_message(message, from, now_ms);
      continue;
    }
    const sls::PairingInfo* info = std::get_if<sls::PairingInfo>(&message.body);
    if (info == nullptr) continue;
    if (message.type == sls::MessageType::PairingHello) {
      apply_pairing_actions(pairing->on_hello(message.sender, *info, now_ms), now_ms);
    } else if (message.type == sls::MessageType::PairingAck) {
      apply_pairing_actions(pairing->on_ack(message.sender, *info, now_ms), now_ms);
    }
  }
  apply_pairing_actions(pairing->on_tick(now_ms), now_ms);
  // A Unit joined to a router stays on its channel. Otherwise pair on the
  // pairing channel, or hunt for a Coordinator that moved to its router's channel,
  // except while Matter commissioning is scanning for or joining the router.
  group_sync->on_commissioning(matter_started && matter_endpoints::commissioning());
  if (!WiFi.isConnected()) {
    const uint8_t channel = pairing->window_open() ? ESPNOW_PAIRING_CHANNEL : group_sync->listen_channel(now_ms);
    if (channel != 0) espnow.set_channel(channel);
  }
  // Apply voice writes, then report back the real state, including clamped
  // values and changes made here (motion, boot restore).
  if (group_sync->matter_enabled()) {
    const bool was_open = group_state.open;
    const sls::Look look_before = group_state.look;
    matter_endpoints::sync(group_state);
    if (was_open && !group_state.open) {
      motion_rules.on_manual_close();
    } else if (group_state.open && !same_look(look_before, group_state.look)) {
      motion_rules.on_explicit_open();
    }
    if (matter_endpoints::commissioned() && group_sync->role() != sls::SyncRole::Coordinator) {
      espnow.send_all(group_sync->on_commissioned(now_ms));
      save_record_if_changed();
      ensure_motion_master();
    }
    run_motion_rules(now_ms);
  } else {
    group_state = group_sync->state();  // the Coordinator owns the state
  }
  espnow.send_all(group_sync->on_local_change(group_state, now_ms));
  espnow.send_all(group_sync->on_tick(now_ms, WiFi.channel()));
  scheduler.on_change(group_state, now_ms);
  if (scheduler.due(now_ms)) {
    save_group_state(group_state);
  }
  const uint32_t phase = group_sync->blink_phase_ms(now_ms);
  const bool lit = sls::blink_lit(group_state.look, group_state.open, phase);
  if (pairing->window_open()) {
    strip.render(pairing_look(now_ms), true);
  } else if (!render_confirmation(now_ms)) {
    strip.render(group_state.look, lit);
  }
  // Short enough that the 50 ms minimum blink period still shows (25 ms on, 25 ms off).
  delay(10);
}
