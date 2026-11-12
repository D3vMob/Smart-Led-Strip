// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#pragma once
#include <cstdint>

#include "group_state.h"
#include "persisted_state.h"

namespace sls {

// Decides when the latest reported state should be written to flash: once it
// has stopped changing for settle_ms and differs from what was last saved.
class SaveScheduler {
 public:
  SaveScheduler(const GroupState& saved, uint32_t settle_ms);
  void on_change(const GroupState& state, uint32_t now_ms);
  bool due(uint32_t now_ms);

 private:
  GroupStateBlob saved_;
  GroupStateBlob latest_;
  uint32_t settle_ms_;
  uint32_t last_change_ms_ = 0;
  bool pending_ = false;
};

}  // namespace sls
