// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#pragma once
#include <cstdint>

#include "group_state.h"

// Maps the device's five Matter endpoints onto the Group state, with no Matter
// dependency: attribute writes become reducer commands, and a Group state
// becomes the attribute values to report back. The adapter reports whatever
// report_for() returns after every change, so the app shows what the Strip
// really does (a 5% dim reads back as the clamped 10%).
//
// Every write function returns true and fills `out` when the write maps to a
// command, false when the write does nothing.
//
// Matter levels, hues and saturations are 0-254; color temperature is mireds.

namespace sls {

// Strip: an extended color light (on/off, level, hue/saturation, color temperature).
bool strip_on_off(bool on, Command& out);
bool strip_level(uint8_t matter_level, Command& out);
bool strip_hue_sat(uint8_t matter_hue, uint8_t matter_saturation, Command& out);
bool strip_color_temp(uint16_t mireds, Command& out);

// Blink: an on/off switch.
bool blink_on_off(bool on, Command& out);

// Blink speed: a dimmable light whose level % sets the blink period. Its own
// on/off attribute controls nothing, and never maps.
bool blink_speed_level(uint8_t matter_level, Command& out);
bool blink_speed_on_off(bool on, Command& out);

// Motion: an on/off switch for presence automation.
bool motion_on_off(bool on, Command& out);

// Random color: fires once on an on write and reports off again, so it can be
// triggered again. The random value is supplied by the caller.
bool random_color_on_off(bool on, uint32_t random_value, Command& out);

struct StripReport {
  bool on;
  uint8_t level;
  uint8_t hue;
  uint8_t saturation;
  uint16_t color_temp_mireds;
  bool color_temp_mode;  // true when color temperature, not hue/saturation, is active
};

struct OnOffReport {
  bool on;
};

struct DimmableReport {
  bool on;
  uint8_t level;
};

struct EndpointReport {
  StripReport strip;
  OnOffReport blink;
  DimmableReport blink_speed;  // always on
  OnOffReport motion;
  OnOffReport random_color;  // always off
};

EndpointReport report_for(const GroupState& state);

}  // namespace sls
