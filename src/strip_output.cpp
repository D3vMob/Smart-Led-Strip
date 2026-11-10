#include "strip_output.h"

namespace {

uint8_t scale(uint8_t channel, uint8_t brightness_pct) {
  return static_cast<uint8_t>(channel * brightness_pct / 100);
}

uint32_t look_color(Adafruit_NeoPixel& pixels, const sls::Look& look) {
  uint32_t full;
  if (look.color_mode == sls::ColorMode::ColorTemp) {
    // Color temperature is shown as plain white until whites get their own conversion.
    full = pixels.Color(255, 255, 255);
  } else {
    const uint16_t hue16 = static_cast<uint16_t>(static_cast<uint32_t>(look.hue_deg) * 65536UL / 360UL);
    full = Adafruit_NeoPixel::gamma32(Adafruit_NeoPixel::ColorHSV(hue16, look.saturation, 255));
  }
  // Gamma corrects the hue only; brightness scales linearly so the 10% minimum stays visible.
  return pixels.Color(scale(full >> 16, look.brightness_pct), scale(full >> 8, look.brightness_pct),
                      scale(full, look.brightness_pct));
}

}  // namespace

StripOutput::StripOutput(uint16_t led_count, int16_t data_pin, neoPixelType pixel_type)
    : pixels_(led_count, data_pin, pixel_type) {}

void StripOutput::begin() {
  pixels_.begin();
  pixels_.clear();
  pixels_.show();
}

void StripOutput::render(const sls::Look& look, bool lit) {
  if (lit) {
    pixels_.fill(look_color(pixels_, look));
  } else {
    pixels_.clear();
  }
  pixels_.show();
}
