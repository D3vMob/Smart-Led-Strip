#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "espnow_codec.h"
#include "group_sync.h"
#include "motion_master.h"

// Loads the paired Group's member MACs saved in flash; empty when nothing valid is stored.
std::vector<sls::Mac> load_group_members();
void save_group_members(const std::vector<sls::Mac>& members);

// Loads the Group secret saved in flash; empty when nothing valid (exactly 32 bytes) is stored.
std::optional<sls::GroupSecret> load_group_secret();
void save_group_secret(const sls::GroupSecret& secret);

// Loads the Unit's saved role, Coordinator and the Coordinator boot last accepted; Undecided with no
// Coordinator when nothing valid is stored.
sls::SyncRecord load_sync_record();
void save_sync_record(const sls::SyncRecord& record);

// Raises this Unit's boot counter in flash and returns it, so every boot signs its sync
// messages with a higher boot count than the one before.
uint32_t next_boot_count();

// Loads the Coordinator's saved Motion master record (Override and Calibration); none when nothing valid is stored.
std::optional<sls::MotionMasterRecord> load_motion_master_record();
void save_motion_master_record(const sls::MotionMasterRecord& record);

// A re-pairing resets Calibration and any Override: clears the whole "motion" namespace.
void clear_motion_master_keys();
