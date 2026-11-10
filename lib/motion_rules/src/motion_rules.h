#pragma once
#include <cstdint>

#include "config.h"

namespace sls {

enum class MotionIntent : uint8_t { None, Open, Close };

// Drives Open/Closed from presence while Motion is enabled. Callers feed it
// events with the current time and apply each Open/Close intent to the Group
// state with Command::open() / Command::close().
//
// A manual close while someone is present holds the Strip Closed until the
// room has been empty for the absence timeout; an explicit dim/color, or
// toggling Motion, clears the hold.
class MotionRules {
 public:
  explicit MotionRules(uint32_t absence_ms = MOTION_ABSENCE_MS);
  // Enabling counts the absence timeout from now when the room is already empty.
  void on_motion_enabled(bool on, uint32_t now_ms);
  MotionIntent on_presence(bool present, uint32_t now_ms);
  MotionIntent on_tick(uint32_t now_ms);
  void on_manual_close();
  void on_explicit_open();

 private:
  uint32_t absence_ms_;
  bool motion_enabled_ = false;
  // Unknown until the first absence is reported, so a Unit without a sensor
  // never closes the Strip on its own.
  bool room_empty_ = false;
  bool absence_pending_ = false;
  bool hold_ = false;
  uint32_t absent_since_ms_ = 0;
};

}  // namespace sls
