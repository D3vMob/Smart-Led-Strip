#pragma once

#include <vector>

#include "espnow_codec.h"
#include "group_sync.h"

// Loads the paired Group's member MACs saved in flash; empty when nothing valid is stored.
std::vector<sls::Mac> load_group_members();
void save_group_members(const std::vector<sls::Mac>& members);

// Loads the Unit's saved sync role; Undecided when nothing valid is stored.
sls::SyncRole load_sync_role();
void save_sync_role(sls::SyncRole role);

// A re-pairing resets Calibration and any Override: clears the whole "motion" namespace.
void clear_motion_master_keys();
