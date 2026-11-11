#include "channel_scan.h"

namespace sls {

uint8_t scan_channel(uint8_t home, uint32_t silent_for_ms) {
  if (silent_for_ms % kScanIntervalMs >= kScanBurstMs) return home;
  const bool home_in_range = home >= 1 && home <= kLastWifiChannel;
  const uint32_t candidates = home_in_range ? kLastWifiChannel - 1 : kLastWifiChannel;
  uint8_t channel = static_cast<uint8_t>(1 + (silent_for_ms / kScanIntervalMs) % candidates);
  if (home_in_range && channel >= home) ++channel;
  return channel;
}

}  // namespace sls
