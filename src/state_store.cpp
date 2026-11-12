// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#include "state_store.h"

#include <Preferences.h>

#include "persisted_state.h"

namespace {

constexpr const char* kNamespace = "sls";
constexpr const char* kKey = "group";

}  // namespace

sls::GroupState load_group_state() {
  uint8_t buffer[sls::kGroupStateBlobSize] = {};
  size_t length = 0;
  Preferences preferences;
  if (preferences.begin(kNamespace, true)) {
    if (preferences.getBytesLength(kKey) <= sizeof(buffer)) {
      length = preferences.getBytes(kKey, buffer, sizeof(buffer));
    }
    preferences.end();
  }
  return sls::decode_group_state(buffer, length);
}

void save_group_state(const sls::GroupState& state) {
  const sls::GroupStateBlob blob = sls::encode_group_state(state);
  Preferences preferences;
  if (preferences.begin(kNamespace, false)) {
    preferences.putBytes(kKey, blob.data(), blob.size());
    preferences.end();
  }
}
