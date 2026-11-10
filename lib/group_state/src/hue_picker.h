#pragma once

#include <stdint.h>

namespace sls {

// Picks a hue in [0, 360) at least 30 degrees (circular) away from current_hue.
// random_value is injected by the caller; the picker is deterministic.
uint16_t pick_hue(uint16_t current_hue, uint32_t random_value);

}  // namespace sls
