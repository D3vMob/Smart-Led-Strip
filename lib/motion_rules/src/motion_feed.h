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
  // Forgets the last presence fed. The next known presence only sets where the room stands in the
  // rules and neither opens nor closes by itself; later changes do.
  void seed_next();
  void on_manual_close();
  void on_explicit_open();

 private:
  MotionRules rules_;
  bool motion_enabled_ = false;
  std::optional<bool> last_presence_;
  bool seeding_ = false;
};

}  // namespace sls
