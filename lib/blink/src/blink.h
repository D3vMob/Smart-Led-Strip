#pragma once

#include <stdint.h>

#include "group_state.h"

namespace sls {

// Blink period in milliseconds for a speed percentage (clamped to 1-100).
uint32_t blink_period_ms(uint8_t speed_pct);

// Whether the Strip is lit at phase_ms into the blink cycle. A closed Strip is
// never lit; an open Strip without Blink is always lit; with Blink it is lit for
// the first half of each period and dark for the second.
bool blink_lit(const Look& look, bool open, uint32_t phase_ms);

// Turns injected time into ms into the current blink cycle. Can be re-aligned
// to a phase given from outside.
class BlinkClock {
 public:
  // Ms into the current cycle, in [0, period_ms).
  uint32_t phase_ms(uint32_t now_ms, uint32_t period_ms) const;

  // From now on, phase_ms(now_ms, p) == phase_ms % p.
  void align(uint32_t now_ms, uint32_t phase_ms);

 private:
  uint32_t epoch_ms_ = 0;
};

}  // namespace sls
