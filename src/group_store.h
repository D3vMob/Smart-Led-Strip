#pragma once

#include <optional>
#include <vector>

#include "espnow_codec.h"
#include "group_sync.h"

// Loads the paired Group's member MACs saved in flash; empty when nothing valid is stored.
std::vector<sls::Mac> load_group_members();
void save_group_members(const std::vector<sls::Mac>& members);

// Loads the Group secret saved in flash; empty when nothing valid (exactly 32 bytes) is stored.
std::optional<sls::GroupSecret> load_group_secret();
void save_group_secret(const sls::GroupSecret& secret);

// Loads the Unit's saved role and Coordinator; Undecided with no Coordinator when nothing valid is stored.
sls::SyncRecord load_sync_record();
void save_sync_record(const sls::SyncRecord& record);

// A re-pairing resets Calibration and any Override: clears the whole "motion" namespace.
void clear_motion_master_keys();
