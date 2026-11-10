#include "blink.h"

namespace sls {

namespace {
constexpr uint8_t kMinSpeedPct = 1;
constexpr uint8_t kMaxSpeedPct = 100;
constexpr uint32_t kMsPerSpeedPct = 50;
}  // namespace

uint32_t blink_period_ms(uint8_t speed_pct) {
  if (speed_pct < kMinSpeedPct) speed_pct = kMinSpeedPct;
  if (speed_pct > kMaxSpeedPct) speed_pct = kMaxSpeedPct;
  return static_cast<uint32_t>(speed_pct) * kMsPerSpeedPct;
}

bool blink_lit(const Look& look, bool open, uint32_t phase_ms) {
  if (!open) return false;
  if (!look.blink_on) return true;
  const uint32_t period = blink_period_ms(look.blink_speed_pct);
  return (phase_ms % period) < period / 2;
}

uint32_t BlinkClock::phase_ms(uint32_t now_ms, uint32_t period_ms) const {
  if (period_ms == 0) return 0;
  return (now_ms - epoch_ms_) % period_ms;
}

void BlinkClock::align(uint32_t now_ms, uint32_t phase_ms) {
  epoch_ms_ = now_ms - phase_ms;
}

}  // namespace sls
