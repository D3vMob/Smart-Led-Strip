// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "group_state.h"

namespace sls {

// version(1) mode(1) hue(2) sat(1) mireds(2) brightness(1) blink_on(1)
// blink_speed(1) open(1) motion(1) checksum(2)
constexpr size_t kGroupStateBlobSize = 14;
using GroupStateBlob = std::array<uint8_t, kGroupStateBlobSize>;

GroupStateBlob encode_group_state(const GroupState& state);
GroupState decode_group_state(const uint8_t* data, size_t length);

}  // namespace sls
