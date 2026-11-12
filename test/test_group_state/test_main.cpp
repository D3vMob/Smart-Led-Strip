// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#include <unity.h>
#include "group_state.h"

void setUp() {}
void tearDown() {}

void test_default_state_is_closed_with_default_look() {
  sls::GroupState state = sls::default_group_state();
  TEST_ASSERT_FALSE(state.open);
  TEST_ASSERT_EQUAL_UINT8(100, state.look.brightness_pct);
  TEST_ASSERT_FALSE(state.look.blink_on);
  TEST_ASSERT_EQUAL_UINT8(20, state.look.blink_speed_pct);
  TEST_ASSERT_FALSE(state.motion_enabled);
}

void test_open_opens_strip_keeping_look() {
  sls::GroupState before = sls::default_group_state();
  sls::GroupState after = sls::apply(before, sls::Command::open());
  TEST_ASSERT_TRUE(after.open);
  TEST_ASSERT_EQUAL_UINT8(before.look.brightness_pct, after.look.brightness_pct);
}

static sls::GroupState open_state_with_custom_look() {
  sls::GroupState state = sls::apply(sls::default_group_state(), sls::Command::open());
  state = sls::apply(state, sls::Command::set_level(55));
  state = sls::apply(state, sls::Command::set_hue_sat(200, 120));
  state.motion_enabled = true;
  state.look.color_mode = sls::ColorMode::ColorTemp;
  state.look.color_temp_mireds = 370;
  state.look.blink_on = true;
  state.look.blink_speed_pct = 70;
  return state;
}

static void assert_same_look(const sls::Look& expected, const sls::Look& actual) {
  TEST_ASSERT_EQUAL_UINT8((uint8_t)expected.color_mode, (uint8_t)actual.color_mode);
  TEST_ASSERT_EQUAL_UINT16(expected.hue_deg, actual.hue_deg);
  TEST_ASSERT_EQUAL_UINT8(expected.saturation, actual.saturation);
  TEST_ASSERT_EQUAL_UINT16(expected.color_temp_mireds, actual.color_temp_mireds);
  TEST_ASSERT_EQUAL_UINT8(expected.brightness_pct, actual.brightness_pct);
  TEST_ASSERT_EQUAL(expected.blink_on, actual.blink_on);
  TEST_ASSERT_EQUAL_UINT8(expected.blink_speed_pct, actual.blink_speed_pct);
}

void test_close_closes_strip_keeping_look() {
  sls::GroupState before = open_state_with_custom_look();
  sls::GroupState after = sls::apply(before, sls::Command::close());
  TEST_ASSERT_FALSE(after.open);
  TEST_ASSERT_TRUE(after.motion_enabled);
  assert_same_look(before.look, after.look);
}

void test_reopen_after_close_shows_same_look() {
  sls::GroupState before = open_state_with_custom_look();
  sls::GroupState closed = sls::apply(before, sls::Command::close());
  sls::GroupState reopened = sls::apply(closed, sls::Command::open());
  TEST_ASSERT_TRUE(reopened.open);
  assert_same_look(before.look, reopened.look);
}

static sls::GroupState opened_default() {
  return sls::apply(sls::default_group_state(), sls::Command::open());
}

static uint8_t brightness_after_set_level(uint8_t pct) {
  sls::GroupState after = sls::apply(opened_default(), sls::Command::set_level(pct));
  TEST_ASSERT_TRUE(after.open);
  return after.look.brightness_pct;
}

void test_set_level_sets_brightness() {
  TEST_ASSERT_EQUAL_UINT8(55, brightness_after_set_level(55));
}

void test_set_level_keeps_range_boundaries() {
  TEST_ASSERT_EQUAL_UINT8(10, brightness_after_set_level(10));
  TEST_ASSERT_EQUAL_UINT8(100, brightness_after_set_level(100));
}

void test_set_level_below_ten_percent_is_raised_to_ten() {
  TEST_ASSERT_EQUAL_UINT8(10, brightness_after_set_level(1));
  TEST_ASSERT_EQUAL_UINT8(10, brightness_after_set_level(9));
}

void test_set_level_above_hundred_is_capped() {
  sls::GroupState dimmed = sls::apply(opened_default(), sls::Command::set_level(40));
  sls::GroupState after = sls::apply(dimmed, sls::Command::set_level(150));
  TEST_ASSERT_EQUAL_UINT8(100, after.look.brightness_pct);
}

void test_set_level_zero_closes_strip_keeping_brightness() {
  sls::GroupState dimmed = sls::apply(opened_default(), sls::Command::set_level(40));
  sls::GroupState after = sls::apply(dimmed, sls::Command::set_level(0));
  TEST_ASSERT_FALSE(after.open);
  TEST_ASSERT_EQUAL_UINT8(40, after.look.brightness_pct);
}

void test_set_level_opens_closed_strip() {
  sls::GroupState after = sls::apply(sls::default_group_state(), sls::Command::set_level(30));
  TEST_ASSERT_TRUE(after.open);
  TEST_ASSERT_EQUAL_UINT8(30, after.look.brightness_pct);
}

void test_set_level_below_ten_opens_closed_strip_at_ten() {
  sls::GroupState after = sls::apply(sls::default_group_state(), sls::Command::set_level(5));
  TEST_ASSERT_TRUE(after.open);
  TEST_ASSERT_EQUAL_UINT8(10, after.look.brightness_pct);
}

void test_set_level_zero_leaves_closed_strip_closed() {
  sls::GroupState after = sls::apply(sls::default_group_state(), sls::Command::set_level(0));
  TEST_ASSERT_FALSE(after.open);
  TEST_ASSERT_EQUAL_UINT8(100, after.look.brightness_pct);
}

void test_set_hue_sat_sets_color_keeping_brightness() {
  sls::GroupState dimmed = sls::apply(opened_default(), sls::Command::set_level(40));
  dimmed.look.blink_on = true;
  dimmed.look.blink_speed_pct = 70;
  dimmed.motion_enabled = true;
  sls::GroupState after = sls::apply(dimmed, sls::Command::set_hue_sat(200, 180));
  TEST_ASSERT_TRUE(after.open);
  TEST_ASSERT_EQUAL_UINT8((uint8_t)sls::ColorMode::HueSat, (uint8_t)after.look.color_mode);
  TEST_ASSERT_EQUAL_UINT16(200, after.look.hue_deg);
  TEST_ASSERT_EQUAL_UINT8(180, after.look.saturation);
  TEST_ASSERT_EQUAL_UINT8(40, after.look.brightness_pct);
  TEST_ASSERT_TRUE(after.look.blink_on);
  TEST_ASSERT_EQUAL_UINT8(70, after.look.blink_speed_pct);
  TEST_ASSERT_TRUE(after.motion_enabled);
}

void test_set_hue_sat_switches_to_hue_sat_mode() {
  sls::GroupState state = opened_default();
  state.look.color_mode = sls::ColorMode::ColorTemp;
  sls::GroupState after = sls::apply(state, sls::Command::set_hue_sat(10, 20));
  TEST_ASSERT_EQUAL_UINT8((uint8_t)sls::ColorMode::HueSat, (uint8_t)after.look.color_mode);
}

void test_set_hue_sat_opens_closed_strip() {
  sls::GroupState after = sls::apply(sls::default_group_state(), sls::Command::set_hue_sat(120, 255));
  TEST_ASSERT_TRUE(after.open);
  TEST_ASSERT_EQUAL_UINT16(120, after.look.hue_deg);
  TEST_ASSERT_EQUAL_UINT8(255, after.look.saturation);
  TEST_ASSERT_EQUAL_UINT8(100, after.look.brightness_pct);
}

void test_set_hue_sat_wraps_hue_past_360() {
  sls::GroupState after = sls::apply(opened_default(), sls::Command::set_hue_sat(370, 100));
  TEST_ASSERT_EQUAL_UINT16(10, after.look.hue_deg);
}

void test_enabling_motion_opens_strip_keeping_look() {
  sls::GroupState closed = sls::apply(open_state_with_custom_look(), sls::Command::close());
  closed.motion_enabled = false;
  sls::GroupState after = sls::apply(closed, sls::Command::set_motion_enabled(true));
  TEST_ASSERT_TRUE(after.motion_enabled);
  TEST_ASSERT_TRUE(after.open);
  assert_same_look(closed.look, after.look);
}

void test_disabling_motion_leaves_open_strip_open() {
  sls::GroupState before = open_state_with_custom_look();
  sls::GroupState after = sls::apply(before, sls::Command::set_motion_enabled(false));
  TEST_ASSERT_FALSE(after.motion_enabled);
  TEST_ASSERT_TRUE(after.open);
  assert_same_look(before.look, after.look);
}

void test_disabling_motion_leaves_closed_strip_closed() {
  sls::GroupState before = sls::apply(open_state_with_custom_look(), sls::Command::close());
  sls::GroupState after = sls::apply(before, sls::Command::set_motion_enabled(false));
  TEST_ASSERT_FALSE(after.motion_enabled);
  TEST_ASSERT_FALSE(after.open);
  assert_same_look(before.look, after.look);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_default_state_is_closed_with_default_look);
  RUN_TEST(test_open_opens_strip_keeping_look);
  RUN_TEST(test_close_closes_strip_keeping_look);
  RUN_TEST(test_reopen_after_close_shows_same_look);
  RUN_TEST(test_set_level_sets_brightness);
  RUN_TEST(test_set_level_keeps_range_boundaries);
  RUN_TEST(test_set_level_below_ten_percent_is_raised_to_ten);
  RUN_TEST(test_set_level_above_hundred_is_capped);
  RUN_TEST(test_set_level_zero_closes_strip_keeping_brightness);
  RUN_TEST(test_set_level_opens_closed_strip);
  RUN_TEST(test_set_level_below_ten_opens_closed_strip_at_ten);
  RUN_TEST(test_set_level_zero_leaves_closed_strip_closed);
  RUN_TEST(test_set_hue_sat_sets_color_keeping_brightness);
  RUN_TEST(test_set_hue_sat_switches_to_hue_sat_mode);
  RUN_TEST(test_set_hue_sat_opens_closed_strip);
  RUN_TEST(test_set_hue_sat_wraps_hue_past_360);
  RUN_TEST(test_enabling_motion_opens_strip_keeping_look);
  RUN_TEST(test_disabling_motion_leaves_open_strip_open);
  RUN_TEST(test_disabling_motion_leaves_closed_strip_closed);
  return UNITY_END();
}
