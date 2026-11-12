// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#include "frame_renderer.h"

#include <cmath>

namespace sls {
namespace {

// Gamma 2.6 correction, one entry per 8-bit channel value.
const uint8_t kGamma8[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3,
    3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 7,
    7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12,
    13, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20,
    20, 21, 21, 22, 22, 23, 24, 24, 25, 25, 26, 27, 27, 28, 29, 29,
    30, 31, 31, 32, 33, 34, 34, 35, 36, 37, 38, 38, 39, 40, 41, 42,
    42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
    58, 59, 60, 61, 62, 63, 64, 65, 66, 68, 69, 70, 71, 72, 73, 75,
    76, 77, 78, 80, 81, 82, 84, 85, 86, 88, 89, 90, 92, 93, 94, 96,
    97, 99, 100, 102, 103, 105, 106, 108, 109, 111, 112, 114, 115, 117, 119, 120,
    122, 124, 125, 127, 129, 130, 132, 134, 136, 137, 139, 141, 143, 145, 146, 148,
    150, 152, 154, 156, 158, 160, 162, 164, 166, 168, 170, 172, 174, 176, 178, 180,
    182, 184, 186, 188, 191, 193, 195, 197, 199, 202, 204, 206, 209, 211, 213, 215,
    218, 220, 223, 225, 227, 230, 232, 235, 237, 240, 242, 245, 247, 250, 252, 255,
};

struct Rgb {
  uint8_t r, g, b;
};

// Full-value HSV to RGB: hue is mapped onto 1530 steps around the colour wheel.
Rgb hsv_to_rgb(uint16_t hue_deg, uint8_t saturation) {
  uint32_t hue = (static_cast<uint32_t>(hue_deg) * 65536u / 360u * 1530u + 32768u) / 65536u;
  uint32_t r, g, b;
  if (hue < 510) {
    b = 0;
    if (hue < 255) { r = 255; g = hue; } else { r = 510 - hue; g = 255; }
  } else if (hue < 1020) {
    r = 0;
    if (hue < 765) { g = 255; b = hue - 510; } else { g = 1020 - hue; b = 255; }
  } else if (hue < 1530) {
    g = 0;
    if (hue < 1275) { r = hue - 1020; b = 255; } else { r = 255; b = 1530 - hue; }
  } else {
    r = 255; g = 0; b = 0;
  }
  uint32_t s1 = 1u + saturation;
  uint32_t s2 = 255u - saturation;
  auto mix = [&](uint32_t c) { return static_cast<uint8_t>(((((c * s1) >> 8) + s2) * 256u) >> 8); };
  return Rgb{mix(r), mix(g), mix(b)};
}

uint8_t clamp_channel(double v) {
  if (v < 0.0) return 0;
  if (v > 255.0) return 255;
  return static_cast<uint8_t>(v);
}

// Display-space RGB white for a colour temperature (Kelvin approximation).
Rgb color_temp_to_rgb(uint16_t mireds) {
  // Looks loaded from storage aren't clamped by the reducer, so clamp here too.
  mireds = clamp_color_temp(mireds);
  double temp = (1000000.0 / mireds) / 100.0;
  double r = temp <= 66 ? 255.0 : 329.698727446 * std::pow(temp - 60, -0.1332047592);
  double g = temp <= 66 ? 99.4708025861 * std::log(temp) - 161.1195681661
                        : 288.1221695283 * std::pow(temp - 60, -0.0755148492);
  double b = temp >= 66 ? 255.0
             : temp <= 19 ? 0.0
                          : 138.5177312231 * std::log(temp - 10) - 305.0447927307;
  return Rgb{clamp_channel(r), clamp_channel(g), clamp_channel(b)};
}

constexpr uint32_t kMilliampsPerChannelAtFull = 20;

// Applies the power cap: scales all channels down so the estimated draw fits the budget.
Frame apply_power_cap(uint8_t r, uint8_t g, uint8_t b, uint16_t led_count, uint16_t max_milliamps) {
  uint32_t estimate =
      (static_cast<uint32_t>(r) + g + b) * kMilliampsPerChannelAtFull * led_count / 255u;
  if (estimate <= max_milliamps) {
    return Frame{r, g, b, 255};
  }
  uint32_t scale = static_cast<uint32_t>(max_milliamps) * 255u / estimate;
  return Frame{static_cast<uint8_t>(r * scale / 255u), static_cast<uint8_t>(g * scale / 255u),
               static_cast<uint8_t>(b * scale / 255u), static_cast<uint8_t>(scale)};
}

// The Look's color at full brightness. Gamma corrects the hue only: color temperature
// whites are already in display space.
Rgb full_brightness_color(const Look& look) {
  if (look.color_mode == ColorMode::ColorTemp) {
    return color_temp_to_rgb(look.color_temp_mireds);
  }
  Rgb c = hsv_to_rgb(look.hue_deg, look.saturation);
  return Rgb{kGamma8[c.r], kGamma8[c.g], kGamma8[c.b]};
}

// Brightness scales linearly so the 10% minimum stays visible.
uint8_t at_brightness(uint8_t channel, uint8_t brightness_pct) {
  return static_cast<uint8_t>(channel * brightness_pct / 100);
}

}  // namespace

Frame render(const Look& look, bool open, uint16_t led_count, uint16_t max_milliamps) {
  if (!open) {
    return Frame{0, 0, 0, 255};
  }
  Rgb c = full_brightness_color(look);
  return apply_power_cap(at_brightness(c.r, look.brightness_pct),
                         at_brightness(c.g, look.brightness_pct),
                         at_brightness(c.b, look.brightness_pct), led_count, max_milliamps);
}

}  // namespace sls
