#pragma once

#include <Adafruit_NeoPixel.h>

#include "group_state.h"

// Shows a Look on the NeoPixel strip: its color at its brightness while lit,
// dark otherwise.
class StripOutput {
 public:
  StripOutput(uint16_t led_count, int16_t data_pin, neoPixelType pixel_type);
  void begin();
  void render(const sls::Look& look, bool lit);

 private:
  Adafruit_NeoPixel pixels_;
};
