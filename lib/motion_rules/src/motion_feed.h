// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#pragma once
#include <cstdint>
#include <optional>

#include "motion_rules.h"

namespace sls {

// Feeds MotionRules from a Unit's loop: follows the Motion enabled flag, passes on presence only
// when it changes, and ticks the rules.
class MotionFeed {
 public:
  explicit MotionFeed(uint32_t absence_ms = MOTION_ABSENCE_MS);
  // Returns the Open or Close intent this call produced, else None.
  MotionIntent feed(bool motion_enabled, std::optional<bool> presence, uint32_t now_ms);
  // Forgets the last presence fed; the next known presence counts as an arrival.
  void forget_presence();
  // Starts the next silence from fresh motion rules (no pending absence or hold carried over). The
  // next known presence only sets where the room stands in the rules and neither opens nor closes
  // by itself; later changes do. `held` says the Group is Closed as the silence begins: a seeding
  // presence then counts as a manual-close hold, so a brief step out and back in does not reopen.
  void seed_next(bool held = false);
  void on_manual_close();
  void on_explicit_open();

 private:
  uint32_t absence_ms_;
  MotionRules rules_;
  bool motion_enabled_ = false;
  std::optional<bool> last_presence_;
  bool seeding_ = false;
  bool seed_held_ = false;
};

}  // namespace sls
