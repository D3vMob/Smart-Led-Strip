// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#include "save_scheduler.h"

namespace sls {

SaveScheduler::SaveScheduler(const GroupState& saved, uint32_t settle_ms)
    : saved_(encode_group_state(saved)), latest_(saved_), settle_ms_(settle_ms) {}

void SaveScheduler::on_change(const GroupState& state, uint32_t now_ms) {
  const GroupStateBlob blob = encode_group_state(state);
  if (blob == latest_) {
    return;
  }
  latest_ = blob;
  last_change_ms_ = now_ms;
  pending_ = true;
}

bool SaveScheduler::due(uint32_t now_ms) {
  if (!pending_ || now_ms - last_change_ms_ < settle_ms_) {
    return false;
  }
  pending_ = false;
  if (latest_ == saved_) {
    return false;
  }
  saved_ = latest_;
  return true;
}

}  // namespace sls
