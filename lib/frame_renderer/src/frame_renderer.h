// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#pragma once
#include <cstdint>
#include "group_state.h"

namespace sls {

// r,g,b: final channel values written to every pixel.
// scale: power-cap factor, 255 = not capped.
struct Frame {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t scale;
};

Frame render(const Look& look, bool open, uint16_t led_count, uint16_t max_milliamps);

}  // namespace sls
