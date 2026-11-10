#include "button_gesture.h"

namespace sls {

Gesture ButtonGesture::update(bool raw, uint32_t now_ms) {
  if (raw != raw_) {
    raw_ = raw;
    raw_since_ms_ = now_ms;
  }
  bool pressed = pressed_;
  if (raw_ != pressed_ && now_ms - raw_since_ms_ >= kDebounceMs) {
    pressed = raw_;
  }
  const uint32_t edge_ms = raw_since_ms_;
  if (pressed && !pressed_) {
    press_start_ms_ = edge_ms;
    long_fired_ = false;
  }
  Gesture released = Gesture::None;
  if (!pressed && pressed_) {
    if (long_fired_) {
      has_first_release_ = false;
    } else if (has_first_release_ && press_start_ms_ - first_release_ms_ <= kDoublePressGapMs) {
      has_first_release_ = false;
      released = Gesture::DoublePress;
    } else {
      has_first_release_ = true;
      first_release_ms_ = edge_ms;
    }
  }
  pressed_ = pressed;
  if (pressed_ && !long_fired_ && now_ms - press_start_ms_ >= kLongPressMs) {
    long_fired_ = true;
    return Gesture::LongPress;
  }
  return released;
}

}  // namespace sls
