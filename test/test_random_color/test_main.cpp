#include <unity.h>
#include "group_state.h"
#include "hue_picker.h"

void setUp() {}
void tearDown() {}

static uint16_t circular_distance(uint16_t a, uint16_t b) {
  uint16_t diff = a > b ? a - b : b - a;
  return diff > 180 ? 360 - diff : diff;
}

void test_pick_hue_is_always_at_least_30_degrees_from_current_hue() {
  const uint32_t randoms[] = {0, 1, 149, 150, 299, 300, 301, 12345, 4000000000u, 0xFFFFFFFFu};
  const uint16_t currents[] = {0, 1, 10, 29, 30, 180, 330, 340, 358, 359};
  for (uint16_t current : currents) {
    for (uint32_t r : randoms) {
      uint16_t picked = sls::pick_hue(current, r);
      TEST_ASSERT_LESS_THAN_UINT16(360, picked);
      TEST_ASSERT_GREATER_OR_EQUAL_UINT16(30, circular_distance(current, picked));
    }
  }
}

void test_pick_hue_reaches_every_hue_of_the_allowed_arc() {
  const uint16_t current = 10;
  bool reached[360] = {false};
  for (uint32_t r = 0; r <= 300; ++r) reached[sls::pick_hue(current, r)] = true;
  for (uint16_t hue = 0; hue < 360; ++hue) {
    bool allowed = circular_distance(current, hue) >= 30;
    TEST_ASSERT_EQUAL(allowed, reached[hue]);
  }
}

void test_random_color_on_hue_sat_look_picks_far_hue_at_full_saturation_and_keeps_rest() {
  sls::GroupState before = sls::default_group_state();
  before.look = sls::Look{sls::ColorMode::HueSat, 200, 120, 333, 55, true, 70};
  before.open = true;

  sls::GroupState after = sls::apply(before, sls::Command::random_color(12345));

  TEST_ASSERT_EQUAL(sls::ColorMode::HueSat, after.look.color_mode);
  TEST_ASSERT_EQUAL_UINT8(255, after.look.saturation);
  TEST_ASSERT_EQUAL_UINT16(sls::pick_hue(200, 12345), after.look.hue_deg);
  TEST_ASSERT_EQUAL_UINT8(55, after.look.brightness_pct);
  TEST_ASSERT_TRUE(after.look.blink_on);
  TEST_ASSERT_EQUAL_UINT8(70, after.look.blink_speed_pct);
  TEST_ASSERT_EQUAL_UINT16(333, after.look.color_temp_mireds);
}

void test_random_color_opens_a_closed_strip_and_keeps_brightness() {
  sls::GroupState before = sls::default_group_state();
  before.look.brightness_pct = 40;
  before.open = false;

  sls::GroupState after = sls::apply(before, sls::Command::random_color(7));

  TEST_ASSERT_TRUE(after.open);
  TEST_ASSERT_EQUAL_UINT8(40, after.look.brightness_pct);
}

void test_random_color_from_color_temp_look_uses_random_value_as_hue() {
  sls::GroupState before = sls::default_group_state();
  before.look.color_mode = sls::ColorMode::ColorTemp;
  before.look.hue_deg = 250;
  before.open = true;

  sls::GroupState after = sls::apply(before, sls::Command::random_color(460));

  TEST_ASSERT_EQUAL(sls::ColorMode::HueSat, after.look.color_mode);
  TEST_ASSERT_EQUAL_UINT8(255, after.look.saturation);
  TEST_ASSERT_EQUAL_UINT16(100, after.look.hue_deg);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_pick_hue_is_always_at_least_30_degrees_from_current_hue);
  RUN_TEST(test_pick_hue_reaches_every_hue_of_the_allowed_arc);
  RUN_TEST(test_random_color_on_hue_sat_look_picks_far_hue_at_full_saturation_and_keeps_rest);
  RUN_TEST(test_random_color_opens_a_closed_strip_and_keeps_brightness);
  RUN_TEST(test_random_color_from_color_temp_look_uses_random_value_as_hue);
  return UNITY_END();
}
