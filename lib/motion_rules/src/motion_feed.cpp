// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#include "motion_feed.h"

namespace sls {

MotionFeed::MotionFeed(uint32_t absence_ms) : absence_ms_(absence_ms), rules_(absence_ms) {}

MotionIntent MotionFeed::feed(bool motion_enabled, std::optional<bool> presence, uint32_t now_ms) {
  if (motion_enabled != motion_enabled_) {
    motion_enabled_ = motion_enabled;
    rules_.on_motion_enabled(motion_enabled, now_ms);
  }
  MotionIntent intent = MotionIntent::None;
  if (presence && presence != last_presence_) {
    const MotionIntent arrival = rules_.on_presence(*presence, now_ms);
    if (!seeding_) intent = arrival;
    if (seeding_ && seed_held_) rules_.on_manual_close();
    seeding_ = false;
    seed_held_ = false;
  }
  last_presence_ = presence;
  const MotionIntent tick = rules_.on_tick(now_ms);
  return intent != MotionIntent::None ? intent : tick;
}

void MotionFeed::forget_presence() {
  last_presence_.reset();
  seeding_ = false;
  seed_held_ = false;
}

void MotionFeed::seed_next(bool held) {
  rules_ = MotionRules(absence_ms_);
  motion_enabled_ = false;
  last_presence_.reset();
  seeding_ = true;
  seed_held_ = held;
}

void MotionFeed::on_manual_close() { rules_.on_manual_close(); }

void MotionFeed::on_explicit_open() { rules_.on_explicit_open(); }

}  // namespace sls
