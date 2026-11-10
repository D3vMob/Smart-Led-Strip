#include "hue_picker.h"

namespace sls {

namespace {

constexpr uint16_t kHueDegrees = 360;
constexpr uint16_t kMinHueShiftDegrees = 30;
// Hues at circular distance >= 30 from the current one form one arc of this many values.
constexpr uint32_t kAllowedHueCount = kHueDegrees - 2 * kMinHueShiftDegrees + 1;

}  // namespace

uint16_t pick_hue(uint16_t current_hue, uint32_t random_value) {
  uint32_t offset = kMinHueShiftDegrees + random_value % kAllowedHueCount;
  return static_cast<uint16_t>((current_hue % kHueDegrees + offset) % kHueDegrees);
}

}  // namespace sls
