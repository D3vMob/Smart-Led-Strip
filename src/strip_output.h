#pragma once

#include <Adafruit_NeoPixel.h>

#include "group_state.h"

// Shows a Look on the NeoPixel strip: every pixel gets the rendered frame's
// color, kept within the strip's current budget, while lit; dark otherwise.
class StripOutput {
 public:
  StripOutput(uint16_t led_count, int16_t data_pin, neoPixelType pixel_type,
              uint16_t max_milliamps);
  void begin();
  void render(const sls::Look& look, bool lit);

 private:
  Adafruit_NeoPixel pixels_;
  uint16_t led_count_;
  uint16_t max_milliamps_;
};
