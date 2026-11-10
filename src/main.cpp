#include <Arduino.h>

#include "config.h"
#include "group_state.h"
#include "ld2410_uart.h"
#include "strip_output.h"

namespace {

using SensorStatus = sls::ld2410::SensorDetection::Status;

StripOutput strip(LED_COUNT, LED_DATA_PIN, LED_PIXEL_TYPE);
Ld2410Uart sensor(Serial1, LD2410_RX_PIN, LD2410_TX_PIN, LD2410_DETECT_TIMEOUT_MS);
sls::GroupState group_state = sls::default_group_state();
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
  // Until Matter drives it, open the Strip on boot so the bench Unit shows its default Look.
  group_state = sls::apply(group_state, sls::Command::open());
}

void loop() {
  uint32_t now_ms = millis();
  sls::ld2410::Presence presence;
  if (sensor.poll(now_ms, presence)) report_presence(presence);
  report_sensor_status(now_ms);
  strip.render(group_state);
  delay(50);
}
