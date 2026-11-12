// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#include <unity.h>
#include "blink.h"
#include "endpoint_mapping.h"

void setUp() {}
void tearDown() {}

void test_strip_on_write_opens_the_strip() {
  sls::Command command;
  TEST_ASSERT_TRUE(sls::strip_on_off(true, command));
  sls::GroupState state = sls::apply(sls::default_group_state(), command);
  TEST_ASSERT_TRUE(sls::report_for(state).strip.on);
}

void test_strip_off_write_closes_the_strip() {
  sls::GroupState open = sls::apply(sls::default_group_state(), sls::Command::open());
  sls::Command command;
  TEST_ASSERT_TRUE(sls::strip_on_off(false, command));
  sls::GroupState state = sls::apply(open, command);
  TEST_ASSERT_FALSE(sls::report_for(state).strip.on);
}

static sls::GroupState write_level(sls::GroupState from, uint8_t level) {
  sls::Command command;
  TEST_ASSERT_TRUE(sls::strip_level(level, command));
  return sls::apply(from, command);
}

void test_strip_level_254_is_full_brightness() {
  sls::GroupState state = write_level(sls::default_group_state(), 254);
  TEST_ASSERT_EQUAL_UINT8(100, state.look.brightness_pct);
  TEST_ASSERT_EQUAL_UINT8(254, sls::report_for(state).strip.level);
}

void test_strip_level_below_minimum_reports_clamped_level() {
  sls::GroupState state = write_level(sls::default_group_state(), 13);
  TEST_ASSERT_EQUAL_UINT8(25, sls::report_for(state).strip.level);
}

void test_strip_level_zero_closes_and_keeps_brightness() {
  sls::GroupState dimmed = write_level(sls::default_group_state(), 127);
  sls::GroupState state = write_level(dimmed, 0);
  TEST_ASSERT_FALSE(sls::report_for(state).strip.on);
  TEST_ASSERT_EQUAL_UINT8(dimmed.look.brightness_pct, state.look.brightness_pct);
}

void test_strip_level_one_does_not_close_the_strip() {
  sls::GroupState state = write_level(sls::default_group_state(), 1);
  TEST_ASSERT_TRUE(sls::report_for(state).strip.on);
}

void test_strip_report_includes_level_when_closed() {
  sls::GroupState dimmed = write_level(sls::default_group_state(), 127);
  sls::GroupState state = write_level(dimmed, 0);
  TEST_ASSERT_EQUAL_UINT8(sls::report_for(dimmed).strip.level, sls::report_for(state).strip.level);
}

static sls::GroupState write_hue_sat(sls::GroupState from, uint8_t hue, uint8_t sat) {
  sls::Command command;
  TEST_ASSERT_TRUE(sls::strip_hue_sat(hue, sat, command));
  return sls::apply(from, command);
}

static sls::GroupState write_color_temp(sls::GroupState from, uint16_t mireds) {
  sls::Command command;
  TEST_ASSERT_TRUE(sls::strip_color_temp(mireds, command));
  return sls::apply(from, command);
}

void test_strip_hue_sat_midpoint_maps_to_opposite_hue_and_full_saturation() {
  sls::GroupState state = write_hue_sat(sls::default_group_state(), 127, 254);
  TEST_ASSERT_EQUAL_UINT16(180, state.look.hue_deg);
  TEST_ASSERT_EQUAL_UINT8(255, state.look.saturation);
}

void test_strip_hue_sat_write_round_trips_in_report() {
  const uint8_t values[] = {0, 1, 127, 253, 254};
  for (uint8_t hue : values) {
    for (uint8_t sat : values) {
      sls::StripReport report = sls::report_for(write_hue_sat(sls::default_group_state(), hue, sat)).strip;
      TEST_ASSERT_EQUAL_UINT8(hue, report.hue);
      TEST_ASSERT_EQUAL_UINT8(sat, report.saturation);
    }
  }
}

void test_strip_hue_sat_write_reports_hue_sat_mode() {
  sls::GroupState ct = write_color_temp(sls::default_group_state(), 370);
  TEST_ASSERT_FALSE(sls::report_for(write_hue_sat(ct, 10, 10)).strip.color_temp_mode);
}

void test_strip_color_temp_write_reports_mireds_and_ct_mode() {
  sls::StripReport report = sls::report_for(write_color_temp(sls::default_group_state(), 370)).strip;
  TEST_ASSERT_EQUAL_UINT16(370, report.color_temp_mireds);
  TEST_ASSERT_TRUE(report.color_temp_mode);
}

void test_strip_color_temp_below_range_reports_clamped_mireds() {
  TEST_ASSERT_EQUAL_UINT16(153, sls::report_for(write_color_temp(sls::default_group_state(), 100)).strip.color_temp_mireds);
}

void test_blink_on_write_turns_blink_on() {
  sls::Command command;
  TEST_ASSERT_TRUE(sls::blink_on_off(true, command));
  sls::GroupState state = sls::apply(sls::default_group_state(), command);
  TEST_ASSERT_TRUE(sls::report_for(state).blink.on);
}

void test_blink_off_write_turns_blink_off() {
  sls::Command on;
  TEST_ASSERT_TRUE(sls::blink_on_off(true, on));
  sls::GroupState blinking = sls::apply(sls::default_group_state(), on);
  sls::Command command;
  TEST_ASSERT_TRUE(sls::blink_on_off(false, command));
  TEST_ASSERT_FALSE(sls::report_for(sls::apply(blinking, command)).blink.on);
}

static sls::GroupState write_blink_speed(sls::GroupState from, uint8_t level) {
  sls::Command command;
  TEST_ASSERT_TRUE(sls::blink_speed_level(level, command));
  return sls::apply(from, command);
}

void test_blink_speed_level_for_20_percent_gives_one_second_period() {
  sls::GroupState state = write_blink_speed(sls::default_group_state(), 51);
  TEST_ASSERT_EQUAL_UINT32(1000, sls::blink_period_ms(state.look.blink_speed_pct));
  TEST_ASSERT_EQUAL_UINT8(51, sls::report_for(state).blink_speed.level);
}

void test_blink_speed_level_zero_reports_clamped_minimum() {
  sls::GroupState state = write_blink_speed(sls::default_group_state(), 0);
  TEST_ASSERT_EQUAL_UINT8(3, sls::report_for(state).blink_speed.level);
}

void test_blink_speed_on_off_writes_never_map() {
  sls::Command command;
  TEST_ASSERT_FALSE(sls::blink_speed_on_off(true, command));
  TEST_ASSERT_FALSE(sls::blink_speed_on_off(false, command));
}

void test_blink_speed_always_reports_on() {
  TEST_ASSERT_TRUE(sls::report_for(sls::default_group_state()).blink_speed.on);
}

void test_motion_on_write_enables_motion_and_opens_the_strip() {
  sls::Command command;
  TEST_ASSERT_TRUE(sls::motion_on_off(true, command));
  sls::EndpointReport report = sls::report_for(sls::apply(sls::default_group_state(), command));
  TEST_ASSERT_TRUE(report.motion.on);
  TEST_ASSERT_TRUE(report.strip.on);
}

void test_motion_off_write_disables_motion() {
  sls::Command on;
  TEST_ASSERT_TRUE(sls::motion_on_off(true, on));
  sls::GroupState enabled = sls::apply(sls::default_group_state(), on);
  sls::Command command;
  TEST_ASSERT_TRUE(sls::motion_on_off(false, command));
  TEST_ASSERT_FALSE(sls::report_for(sls::apply(enabled, command)).motion.on);
}

void test_random_color_on_write_changes_hue_and_opens_the_strip() {
  sls::GroupState before = sls::default_group_state();
  sls::Command command;
  TEST_ASSERT_TRUE(sls::random_color_on_off(true, 123456789u, command));
  sls::GroupState after = sls::apply(before, command);
  TEST_ASSERT_NOT_EQUAL_UINT16(before.look.hue_deg, after.look.hue_deg);
  TEST_ASSERT_TRUE(sls::report_for(after).strip.on);
}

void test_random_color_off_write_never_maps() {
  sls::Command command;
  TEST_ASSERT_FALSE(sls::random_color_on_off(false, 123456789u, command));
}

void test_random_color_reports_off_right_after_firing() {
  sls::Command command;
  TEST_ASSERT_TRUE(sls::random_color_on_off(true, 123456789u, command));
  TEST_ASSERT_FALSE(sls::report_for(sls::apply(sls::default_group_state(), command)).random_color.on);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_strip_on_write_opens_the_strip);
  RUN_TEST(test_strip_off_write_closes_the_strip);
  RUN_TEST(test_strip_level_254_is_full_brightness);
  RUN_TEST(test_strip_level_below_minimum_reports_clamped_level);
  RUN_TEST(test_strip_level_zero_closes_and_keeps_brightness);
  RUN_TEST(test_strip_level_one_does_not_close_the_strip);
  RUN_TEST(test_strip_report_includes_level_when_closed);
  RUN_TEST(test_strip_hue_sat_midpoint_maps_to_opposite_hue_and_full_saturation);
  RUN_TEST(test_strip_hue_sat_write_round_trips_in_report);
  RUN_TEST(test_strip_hue_sat_write_reports_hue_sat_mode);
  RUN_TEST(test_strip_color_temp_write_reports_mireds_and_ct_mode);
  RUN_TEST(test_strip_color_temp_below_range_reports_clamped_mireds);
  RUN_TEST(test_blink_on_write_turns_blink_on);
  RUN_TEST(test_blink_off_write_turns_blink_off);
  RUN_TEST(test_blink_speed_level_for_20_percent_gives_one_second_period);
  RUN_TEST(test_blink_speed_level_zero_reports_clamped_minimum);
  RUN_TEST(test_blink_speed_on_off_writes_never_map);
  RUN_TEST(test_blink_speed_always_reports_on);
  RUN_TEST(test_motion_on_write_enables_motion_and_opens_the_strip);
  RUN_TEST(test_motion_off_write_disables_motion);
  RUN_TEST(test_random_color_on_write_changes_hue_and_opens_the_strip);
  RUN_TEST(test_random_color_off_write_never_maps);
  RUN_TEST(test_random_color_reports_off_right_after_firing);
  return UNITY_END();
}
