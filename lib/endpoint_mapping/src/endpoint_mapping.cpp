// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#include "endpoint_mapping.h"

namespace sls {

namespace {

constexpr uint32_t kMaxMatterValue = 254;
constexpr uint32_t kMaxHueDeg = 359;
constexpr uint32_t kMaxSaturation = 255;

// Rescales value from 0..from_max to 0..to_max, rounding to nearest. Between
// two ranges this inverts exactly whenever to_max >= from_max.
uint32_t rescale(uint32_t value, uint32_t from_max, uint32_t to_max) {
  return (value * to_max + from_max / 2) / from_max;
}

// Any nonzero level maps to at least 1%, so a tiny dim never reads as a close.
uint8_t level_to_pct(uint8_t level) {
  if (level == 0) return 0;
  const uint32_t pct = rescale(level, kMaxMatterValue, kMaxLevelPct);
  return static_cast<uint8_t>(pct == 0 ? 1 : pct);
}

uint8_t pct_to_level(uint8_t pct) { return static_cast<uint8_t>(rescale(pct, kMaxLevelPct, kMaxMatterValue)); }

}  // namespace

bool strip_on_off(bool on, Command& out) {
  out = on ? Command::open() : Command::close();
  return true;
}

bool strip_level(uint8_t matter_level, Command& out) {
  out = Command::set_level(level_to_pct(matter_level));
  return true;
}

bool strip_hue_sat(uint8_t matter_hue, uint8_t matter_saturation, Command& out) {
  out = Command::set_hue_sat(static_cast<uint16_t>(rescale(matter_hue, kMaxMatterValue, kMaxHueDeg)),
                             static_cast<uint8_t>(rescale(matter_saturation, kMaxMatterValue, kMaxSaturation)));
  return true;
}

bool strip_color_temp(uint16_t mireds, Command& out) {
  out = Command::set_color_temp(mireds);
  return true;
}

bool blink_on_off(bool on, Command& out) {
  out = Command::set_blink(on);
  return true;
}

bool blink_speed_level(uint8_t matter_level, Command& out) {
  out = Command::set_blink_speed(level_to_pct(matter_level));
  return true;
}

bool blink_speed_on_off(bool, Command&) { return false; }

bool motion_on_off(bool on, Command& out) {
  out = Command::set_motion_enabled(on);
  return true;
}

bool random_color_on_off(bool on, uint32_t random_value, Command& out) {
  if (!on) return false;
  out = Command::random_color(random_value);
  return true;
}

EndpointReport report_for(const GroupState& state) {
  const Look& look = state.look;
  EndpointReport report{};
  report.strip.on = state.open;
  report.strip.level = pct_to_level(look.brightness_pct);
  report.strip.hue = static_cast<uint8_t>(rescale(look.hue_deg, kMaxHueDeg, kMaxMatterValue));
  report.strip.saturation = static_cast<uint8_t>(rescale(look.saturation, kMaxSaturation, kMaxMatterValue));
  report.strip.color_temp_mireds = look.color_temp_mireds;
  report.strip.color_temp_mode = look.color_mode == ColorMode::ColorTemp;
  report.blink.on = look.blink_on;
  report.blink_speed.on = true;
  report.blink_speed.level = pct_to_level(look.blink_speed_pct);
  report.motion.on = state.motion_enabled;
  report.random_color.on = false;
  return report;
}

}  // namespace sls
