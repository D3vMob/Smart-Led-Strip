// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#pragma once
#include <cstdint>

namespace sls {

constexpr uint32_t kDebounceMs = 30;
constexpr uint32_t kLongPressMs = 2000;
constexpr uint32_t kDoublePressGapMs = 400;

enum class Gesture : uint8_t { None, LongPress, DoublePress };

class ButtonGesture {
 public:
  // Called every loop with the raw pin level, already inverted so true means pressed.
  Gesture update(bool pressed, uint32_t now_ms);

 private:
  bool raw_ = false;
  uint32_t raw_since_ms_ = 0;
  bool pressed_ = false;
  bool long_fired_ = false;
  bool has_first_release_ = false;
  uint32_t press_start_ms_ = 0;
  uint32_t first_release_ms_ = 0;
};

}  // namespace sls
