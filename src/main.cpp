#include <Arduino.h>

#include "blink.h"
#include "config.h"
#include "group_state.h"
#include "ld2410_uart.h"
#include "matter_endpoints.h"
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

}  // namespace

void setup() {
  Serial.begin(115200);
  strip.begin();
  sensor.begin(millis());
  // The Strip comes back the way it was left; a fresh Unit starts in the defaults (Closed).
  group_state = load_group_state();
  scheduler = sls::SaveScheduler(group_state, PERSIST_SETTLE_MS);
  matter_endpoints::begin(group_state);
}

void loop() {
  const uint32_t now_ms = millis();
  sls::ld2410::Presence presence;
  if (sensor.poll(now_ms, presence)) report_presence(presence);
  report_sensor_status(now_ms);
  // Apply voice writes, then report back the real state, including clamped
  // values and changes made here (motion, boot restore).
  matter_endpoints::sync(group_state);
  scheduler.on_change(group_state, now_ms);
  if (scheduler.due(now_ms)) {
    save_group_state(group_state);
  }
  const uint32_t phase = blink_clock.phase_ms(now_ms, sls::blink_period_ms(group_state.look.blink_speed_pct));
  const bool lit = sls::blink_lit(group_state.look, group_state.open, phase);
  strip.render(group_state.look, lit);
  // Short enough that the 50 ms minimum blink period still shows (25 ms on, 25 ms off).
  delay(10);
}
