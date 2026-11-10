#pragma once

#include <vector>

#include "espnow_codec.h"

// Loads the paired Group's member MACs saved in flash; empty when nothing valid is stored.
std::vector<sls::Mac> load_group_members();
void save_group_members(const std::vector<sls::Mac>& members);

// A re-pairing resets Calibration and any Override: clears the whole "motion" namespace.
void clear_motion_master_keys();
