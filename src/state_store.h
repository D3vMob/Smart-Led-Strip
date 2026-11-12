// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#pragma once

#include "group_state.h"

// Loads the Group state saved in flash; defaults when nothing valid is stored.
sls::GroupState load_group_state();
void save_group_state(const sls::GroupState& state);
