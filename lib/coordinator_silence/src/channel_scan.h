// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#pragma once
#include <cstdint>

namespace sls {

// A Unit that lost its Coordinator listens on its home channel and, for a burst at the start of
// every kScanIntervalMs, on one other Wi-Fi channel. Bursts walk through the other channels in turn.
constexpr uint32_t kScanIntervalMs = 30000;
// One burst; longer than a Heartbeat interval, so a Heartbeat on the burst's channel is always heard.
constexpr uint32_t kScanBurstMs = 3000;
constexpr uint8_t kLastWifiChannel = 13;

// The channel to listen on after silent_for_ms of Coordinator silence (counted from when it became
// silent). Outside a burst: home. Burst k (silent_for_ms in [k * kScanIntervalMs, k * kScanIntervalMs
// + kScanBurstMs)): the k-th channel of 1..kLastWifiChannel skipping home, wrapping after the last one.
uint8_t scan_channel(uint8_t home, uint32_t silent_for_ms);

}  // namespace sls
