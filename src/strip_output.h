#pragma once

#include <Adafruit_NeoPixel.h>

#include "group_state.h"

// Shows a Group state on the NeoPixel strip: the Look's color at its brightness
// while Open, dark while Closed.
class StripOutput {
 public:
  StripOutput(uint16_t led_count, int16_t data_pin, neoPixelType pixel_type);
  void begin();
  void render(const sls::GroupState& state);

 private:
  Adafruit_NeoPixel pixels_;
};
