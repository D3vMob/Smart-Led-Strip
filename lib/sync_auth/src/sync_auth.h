// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "espnow_codec.h"

namespace sls {

using Sha256Digest = std::array<uint8_t, 32>;

// SHA-256 of `data` (FIPS 180-4).
Sha256Digest sha256(const uint8_t* data, size_t len);

// HMAC-SHA256 (RFC 2104). Keys longer than the 64-byte block are hashed first.
Sha256Digest hmac_sha256(const uint8_t* key, size_t key_len, const uint8_t* data, size_t len);

using GroupSecret = std::array<uint8_t, 32>;

// Sets message.auth.tag: the first kSyncTagBytes of HMAC-SHA256(secret, encoded frame with the tag zeroed).
void sign(Message& message, const GroupSecret& secret);

// True only for a State, Heartbeat, MatterSilent, Presence or OverrideClaim whose tag matches. Compared in constant time.
bool verify(const Message& message, const GroupSecret& secret);

}  // namespace sls
