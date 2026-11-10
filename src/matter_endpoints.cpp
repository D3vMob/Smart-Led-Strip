#include "matter_endpoints.h"

#include <Arduino.h>
#include <Matter.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "endpoint_mapping.h"

namespace matter_endpoints {

namespace {

MatterEnhancedColorLight strip;
MatterOnOffLight blink;
MatterDimmableLight blink_speed;
MatterOnOffLight motion;
MatterOnOffLight random_color;

// Commands from Matter callbacks, waiting for sync() on the loop task.
constexpr UBaseType_t kPendingWrites = 16;
QueueHandle_t pending = nullptr;

// Queues the command a write maps to. Always accepts the write: sync()
// corrects the attribute afterwards when the state differs from the request.
bool apply(bool maps, const sls::Command& command) {
  if (maps) xQueueSend(pending, &command, 0);
  return true;
}

}  // namespace

void begin(const sls::GroupState& initial_state) {
  pending = xQueueCreate(kPendingWrites, sizeof(sls::Command));
  const sls::EndpointReport initial = sls::report_for(initial_state);

  strip.begin(initial.strip.on, espHsvColor_t{initial.strip.hue, initial.strip.saturation, initial.strip.level},
              initial.strip.level, initial.strip.color_temp_mireds);
  blink.begin(initial.blink.on);
  blink_speed.begin(initial.blink_speed.on, initial.blink_speed.level);
  motion.begin(initial.motion.on);
  random_color.begin(initial.random_color.on);

  strip.onChangeOnOff([](bool on) {
    sls::Command command;
    return apply(sls::strip_on_off(on, command), command);
  });
  strip.onChangeBrightness([](uint8_t level) {
    sls::Command command;
    return apply(sls::strip_level(level, command), command);
  });
  strip.onChangeColorHSV([](espHsvColor_t hsv) {
    sls::Command command;
    return apply(sls::strip_hue_sat(static_cast<uint8_t>(hsv.h), hsv.s, command), command);
  });
  strip.onChangeColorTemperature([](uint16_t mireds) {
    sls::Command command;
    return apply(sls::strip_color_temp(mireds, command), command);
  });
  blink.onChangeOnOff([](bool on) {
    sls::Command command;
    return apply(sls::blink_on_off(on, command), command);
  });
  blink_speed.onChangeOnOff([](bool on) {
    sls::Command command;
    return apply(sls::blink_speed_on_off(on, command), command);
  });
  blink_speed.onChangeBrightness([](uint8_t level) {
    sls::Command command;
    return apply(sls::blink_speed_level(level, command), command);
  });
  motion.onChangeOnOff([](bool on) {
    sls::Command command;
    return apply(sls::motion_on_off(on, command), command);
  });
  random_color.onChangeOnOff([](bool on) {
    sls::Command command;
    return apply(sls::random_color_on_off(on, esp_random(), command), command);
  });

  // Commissioning is the standard BLE flow: the phone app hands over the Wi-Fi
  // credentials, so no SSID is set here.
  Matter.begin();
  if (!Matter.isDeviceCommissioned()) {
    Serial.println("Matter: not commissioned. Add the Unit in Alexa or Google Home with:");
    Serial.printf("  QR code URL: %s\n", Matter.getOnboardingQRCodeUrl().c_str());
    Serial.printf("  Manual pairing code: %s\n", Matter.getManualPairingCode().c_str());
  }
}

void sync(sls::GroupState& state) {
  sls::Command command;
  while (xQueueReceive(pending, &command, 0) == pdTRUE) state = sls::apply(state, command);

  const sls::EndpointReport values = sls::report_for(state);
  strip.setOnOff(values.strip.on);
  strip.setBrightness(values.strip.level);
  if (values.strip.color_temp_mode) {
    strip.setColorTemperature(values.strip.color_temp_mireds);
  } else {
    strip.setColorHSV(espHsvColor_t{values.strip.hue, values.strip.saturation, values.strip.level});
  }
  blink.setOnOff(values.blink.on);
  blink_speed.setOnOff(values.blink_speed.on);
  blink_speed.setBrightness(values.blink_speed.level);
  motion.setOnOff(values.motion.on);
  random_color.setOnOff(values.random_color.on);
}

}  // namespace matter_endpoints
