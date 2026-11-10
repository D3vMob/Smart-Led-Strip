#include "strip_output.h"

namespace {

uint32_t look_color(Adafruit_NeoPixel& pixels, const sls::Look& look) {
  const uint8_t value = static_cast<uint8_t>(look.brightness_pct * 255 / 100);
  if (look.color_mode == sls::ColorMode::ColorTemp) {
    // Color temperature is shown as plain white until whites get their own conversion.
    return pixels.Color(value, value, value);
  }
  const uint16_t hue16 = static_cast<uint16_t>(static_cast<uint32_t>(look.hue_deg) * 65536UL / 360UL);
  return Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(hue16, look.saturation, value));
}

}  // namespace

StripOutput::StripOutput(uint16_t led_count, int16_t data_pin, neoPixelType pixel_type)
    : pixels_(led_count, data_pin, pixel_type) {}

void StripOutput::begin() {
  pixels_.begin();
  pixels_.clear();
  pixels_.show();
}

void StripOutput::render(const sls::GroupState& state) {
  if (state.open) {
    pixels_.fill(look_color(pixels_, state.look));
  } else {
    pixels_.clear();
  }
  pixels_.show();
}
