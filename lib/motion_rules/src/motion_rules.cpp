#include "motion_rules.h"

namespace sls {

MotionRules::MotionRules(uint32_t absence_ms) : absence_ms_(absence_ms) {}

void MotionRules::on_motion_enabled(bool on, uint32_t now_ms) {
  motion_enabled_ = on;
  if (on) hold_ = false;  // enabling opens the Strip: an explicit open
  absence_pending_ = on && room_empty_;
  if (absence_pending_) absent_since_ms_ = now_ms;
}

MotionIntent MotionRules::on_presence(bool present, uint32_t now_ms) {
  const bool newly_absent = !present && !room_empty_;
  room_empty_ = !present;
  const bool arrived = present && !was_present_;
  was_present_ = present;
  if (motion_enabled_) {
    if (present) {
      absence_pending_ = false;
    } else if (newly_absent) {
      absence_pending_ = true;
      absent_since_ms_ = now_ms;
    }
  }
  if (arrived && motion_enabled_ && !hold_) return MotionIntent::Open;
  return MotionIntent::None;
}

MotionIntent MotionRules::on_tick(uint32_t now_ms) {
  if (motion_enabled_ && absence_pending_ &&
      static_cast<uint32_t>(now_ms - absent_since_ms_) >= absence_ms_) {
    absence_pending_ = false;
    if (hold_) {
      hold_ = false;
      return MotionIntent::None;
    }
    return MotionIntent::Close;
  }
  return MotionIntent::None;
}

void MotionRules::on_manual_close() { hold_ = motion_enabled_ && !room_empty_; }

void MotionRules::on_explicit_open() { hold_ = false; }

}  // namespace sls
