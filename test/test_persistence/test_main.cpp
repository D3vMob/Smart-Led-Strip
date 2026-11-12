// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#include <unity.h>
#include "persisted_state.h"
#include "save_scheduler.h"

void setUp() {}
void tearDown() {}

static void assert_same_state(const sls::GroupState& expected, const sls::GroupState& actual) {
  TEST_ASSERT_EQUAL_UINT8((uint8_t)expected.look.color_mode, (uint8_t)actual.look.color_mode);
  TEST_ASSERT_EQUAL_UINT16(expected.look.hue_deg, actual.look.hue_deg);
  TEST_ASSERT_EQUAL_UINT8(expected.look.saturation, actual.look.saturation);
  TEST_ASSERT_EQUAL_UINT16(expected.look.color_temp_mireds, actual.look.color_temp_mireds);
  TEST_ASSERT_EQUAL_UINT8(expected.look.brightness_pct, actual.look.brightness_pct);
  TEST_ASSERT_EQUAL(expected.look.blink_on, actual.look.blink_on);
  TEST_ASSERT_EQUAL_UINT8(expected.look.blink_speed_pct, actual.look.blink_speed_pct);
  TEST_ASSERT_EQUAL(expected.open, actual.open);
  TEST_ASSERT_EQUAL(expected.motion_enabled, actual.motion_enabled);
}

static sls::GroupState non_default_state() {
  sls::GroupState state{};
  state.look.color_mode = sls::ColorMode::ColorTemp;
  state.look.hue_deg = 200;
  state.look.saturation = 120;
  state.look.color_temp_mireds = 370;
  state.look.brightness_pct = 55;
  state.look.blink_on = true;
  state.look.blink_speed_pct = 70;
  state.open = true;
  state.motion_enabled = true;
  return state;
}

void test_decoding_an_encoded_state_restores_every_field() {
  const sls::GroupState state = non_default_state();
  sls::GroupStateBlob blob = sls::encode_group_state(state);
  assert_same_state(state, sls::decode_group_state(blob.data(), blob.size()));
}

void test_decoding_an_encoded_state_restores_opposite_field_values() {
  sls::GroupState state{};
  state.look.color_mode = sls::ColorMode::HueSat;
  state.look.hue_deg = 359;
  state.look.saturation = 33;
  state.look.color_temp_mireds = 153;
  state.look.brightness_pct = 10;
  state.look.blink_on = false;
  state.look.blink_speed_pct = 5;
  state.open = false;
  state.motion_enabled = false;

  sls::GroupStateBlob blob = sls::encode_group_state(state);
  assert_same_state(state, sls::decode_group_state(blob.data(), blob.size()));
}

void test_decoding_a_blob_with_a_bad_checksum_gives_defaults() {
  sls::GroupStateBlob blob = sls::encode_group_state(non_default_state());
  blob[7] ^= 0x01;
  assert_same_state(sls::default_group_state(), sls::decode_group_state(blob.data(), blob.size()));
}

static void resign(sls::GroupStateBlob& blob) {
  uint16_t sum1 = 0;
  uint16_t sum2 = 0;
  for (size_t i = 0; i < blob.size() - 2; ++i) {
    sum1 = (sum1 + blob[i]) % 255;
    sum2 = (sum2 + sum1) % 255;
  }
  blob[blob.size() - 2] = static_cast<uint8_t>(sum1);
  blob[blob.size() - 1] = static_cast<uint8_t>(sum2);
}

void test_decoding_a_blob_with_an_unknown_version_gives_defaults() {
  sls::GroupStateBlob blob = sls::encode_group_state(non_default_state());
  blob[0] = 2;
  resign(blob);
  assert_same_state(sls::default_group_state(), sls::decode_group_state(blob.data(), blob.size()));
}

void test_decoding_data_that_is_too_short_gives_defaults() {
  sls::GroupStateBlob blob = sls::encode_group_state(non_default_state());
  assert_same_state(sls::default_group_state(),
                    sls::decode_group_state(blob.data(), blob.size() - 1));
}

void test_decoding_no_data_gives_defaults() {
  assert_same_state(sls::default_group_state(), sls::decode_group_state(nullptr, 0));
}

void test_decoding_an_unknown_color_mode_gives_defaults() {
  sls::GroupStateBlob blob = sls::encode_group_state(non_default_state());
  blob[1] = 7;
  resign(blob);
  assert_same_state(sls::default_group_state(), sls::decode_group_state(blob.data(), blob.size()));
}

void test_decoding_a_hue_of_360_or_more_gives_defaults() {
  sls::GroupStateBlob blob = sls::encode_group_state(non_default_state());
  blob[2] = 0x68;
  blob[3] = 0x01;
  resign(blob);
  assert_same_state(sls::default_group_state(), sls::decode_group_state(blob.data(), blob.size()));
}

void test_decoding_a_brightness_above_100_gives_defaults() {
  sls::GroupStateBlob blob = sls::encode_group_state(non_default_state());
  blob[7] = 101;
  resign(blob);
  assert_same_state(sls::default_group_state(), sls::decode_group_state(blob.data(), blob.size()));
}

void test_decoding_a_brightness_below_10_gives_defaults() {
  sls::GroupStateBlob blob = sls::encode_group_state(non_default_state());
  blob[7] = 9;
  resign(blob);
  assert_same_state(sls::default_group_state(), sls::decode_group_state(blob.data(), blob.size()));
}

void test_decoding_a_blink_speed_above_100_gives_defaults() {
  sls::GroupStateBlob blob = sls::encode_group_state(non_default_state());
  blob[9] = 101;
  resign(blob);
  assert_same_state(sls::default_group_state(), sls::decode_group_state(blob.data(), blob.size()));
}

void test_decoding_a_blink_speed_of_0_gives_defaults() {
  sls::GroupStateBlob blob = sls::encode_group_state(non_default_state());
  blob[9] = 0;
  resign(blob);
  assert_same_state(sls::default_group_state(), sls::decode_group_state(blob.data(), blob.size()));
}

void test_decoding_a_boolean_byte_other_than_0_or_1_gives_defaults() {
  sls::GroupStateBlob blob = sls::encode_group_state(non_default_state());
  blob[10] = 2;
  resign(blob);
  assert_same_state(sls::default_group_state(), sls::decode_group_state(blob.data(), blob.size()));
}

void test_a_single_change_is_due_once_the_settle_time_has_passed() {
  sls::SaveScheduler scheduler(sls::default_group_state(), 3000);
  scheduler.on_change(non_default_state(), 0);
  TEST_ASSERT_FALSE(scheduler.due(2999));
  TEST_ASSERT_TRUE(scheduler.due(3000));
}

void test_a_burst_of_changes_is_due_once_after_the_last_one_settles() {
  sls::GroupState state = non_default_state();
  sls::SaveScheduler scheduler(sls::default_group_state(), 3000);
  const uint32_t times[] = {0, 1000, 2000, 2900};
  for (uint32_t t : times) {
    state.look.brightness_pct = static_cast<uint8_t>(20 + t / 100);
    scheduler.on_change(state, t);
  }
  TEST_ASSERT_FALSE(scheduler.due(5899));
  TEST_ASSERT_TRUE(scheduler.due(5900));
  TEST_ASSERT_FALSE(scheduler.due(5901));
  TEST_ASSERT_FALSE(scheduler.due(9000));
}

void test_a_change_back_to_the_saved_state_is_never_due() {
  sls::SaveScheduler scheduler(non_default_state(), 3000);
  scheduler.on_change(sls::default_group_state(), 0);
  scheduler.on_change(non_default_state(), 500);
  TEST_ASSERT_FALSE(scheduler.due(3500));
  TEST_ASSERT_FALSE(scheduler.due(10000));
}

void test_a_saved_state_reported_again_is_not_due_again() {
  sls::SaveScheduler scheduler(sls::default_group_state(), 3000);
  scheduler.on_change(non_default_state(), 0);
  TEST_ASSERT_TRUE(scheduler.due(3000));
  scheduler.on_change(non_default_state(), 4000);
  TEST_ASSERT_FALSE(scheduler.due(7000));
}

void test_without_any_change_nothing_is_ever_due() {
  sls::SaveScheduler scheduler(non_default_state(), 3000);
  TEST_ASSERT_FALSE(scheduler.due(0));
  TEST_ASSERT_FALSE(scheduler.due(3000));
  TEST_ASSERT_FALSE(scheduler.due(100000));
}

void test_a_change_is_due_after_the_millisecond_counter_wraps() {
  sls::SaveScheduler scheduler(sls::default_group_state(), 3000);
  scheduler.on_change(non_default_state(), 0xFFFFF000u);
  TEST_ASSERT_FALSE(scheduler.due(0xFFFFF000u + 2999u));
  TEST_ASSERT_TRUE(scheduler.due(0xFFFFF000u + 3000u));
}

void test_reporting_the_same_state_again_does_not_delay_its_save() {
  sls::SaveScheduler scheduler(sls::default_group_state(), 3000);
  scheduler.on_change(non_default_state(), 0);
  for (uint32_t t = 50; t < 3000; t += 50) {
    scheduler.on_change(non_default_state(), t);
    TEST_ASSERT_FALSE(scheduler.due(t));
  }
  scheduler.on_change(non_default_state(), 3000);
  TEST_ASSERT_TRUE(scheduler.due(3000));
}

void test_an_unchanged_state_reported_repeatedly_is_never_due() {
  sls::SaveScheduler scheduler(non_default_state(), 3000);
  for (uint32_t t = 0; t <= 10000; t += 50) {
    scheduler.on_change(non_default_state(), t);
    TEST_ASSERT_FALSE(scheduler.due(t));
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_decoding_an_encoded_state_restores_every_field);
  RUN_TEST(test_decoding_an_encoded_state_restores_opposite_field_values);
  RUN_TEST(test_decoding_a_blob_with_a_bad_checksum_gives_defaults);
  RUN_TEST(test_decoding_a_blob_with_an_unknown_version_gives_defaults);
  RUN_TEST(test_decoding_data_that_is_too_short_gives_defaults);
  RUN_TEST(test_decoding_no_data_gives_defaults);
  RUN_TEST(test_decoding_an_unknown_color_mode_gives_defaults);
  RUN_TEST(test_decoding_a_hue_of_360_or_more_gives_defaults);
  RUN_TEST(test_decoding_a_brightness_above_100_gives_defaults);
  RUN_TEST(test_decoding_a_brightness_below_10_gives_defaults);
  RUN_TEST(test_decoding_a_blink_speed_above_100_gives_defaults);
  RUN_TEST(test_decoding_a_blink_speed_of_0_gives_defaults);
  RUN_TEST(test_decoding_a_boolean_byte_other_than_0_or_1_gives_defaults);
  RUN_TEST(test_a_single_change_is_due_once_the_settle_time_has_passed);
  RUN_TEST(test_a_burst_of_changes_is_due_once_after_the_last_one_settles);
  RUN_TEST(test_a_change_back_to_the_saved_state_is_never_due);
  RUN_TEST(test_a_saved_state_reported_again_is_not_due_again);
  RUN_TEST(test_without_any_change_nothing_is_ever_due);
  RUN_TEST(test_a_change_is_due_after_the_millisecond_counter_wraps);
  RUN_TEST(test_reporting_the_same_state_again_does_not_delay_its_save);
  RUN_TEST(test_an_unchanged_state_reported_repeatedly_is_never_due);
  return UNITY_END();
}
