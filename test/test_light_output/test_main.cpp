#include <unity.h>
#include "frame_renderer.h"
#include "group_state.h"

void setUp() {}
void tearDown() {}

void test_closed_strip_renders_black() {
  sls::Look look{sls::ColorMode::HueSat, 120, 100, 0, 100, false, 20};
  sls::Frame frame = sls::render(look, false, 60, 1000);
  TEST_ASSERT_EQUAL_UINT8(0, frame.r);
  TEST_ASSERT_EQUAL_UINT8(0, frame.g);
  TEST_ASSERT_EQUAL_UINT8(0, frame.b);
}

void test_open_full_brightness_full_saturation_renders_pure_primaries() {
  sls::Look red{sls::ColorMode::HueSat, 0, 255, 0, 100, false, 20};
  sls::Look green{sls::ColorMode::HueSat, 120, 255, 0, 100, false, 20};
  sls::Look blue{sls::ColorMode::HueSat, 240, 255, 0, 100, false, 20};
  sls::Frame fr = sls::render(red, true, 1, 10000);
  sls::Frame fg = sls::render(green, true, 1, 10000);
  sls::Frame fb = sls::render(blue, true, 1, 10000);
  TEST_ASSERT_EQUAL_UINT8(255, fr.r);
  TEST_ASSERT_EQUAL_UINT8(0, fr.g);
  TEST_ASSERT_EQUAL_UINT8(0, fr.b);
  TEST_ASSERT_EQUAL_UINT8(0, fg.r);
  TEST_ASSERT_EQUAL_UINT8(255, fg.g);
  TEST_ASSERT_EQUAL_UINT8(0, fg.b);
  TEST_ASSERT_EQUAL_UINT8(0, fb.r);
  TEST_ASSERT_EQUAL_UINT8(0, fb.g);
  TEST_ASSERT_EQUAL_UINT8(255, fb.b);
}

void test_brightness_scales_channels_linearly_and_keeps_minimum_visible() {
  sls::Look half{sls::ColorMode::HueSat, 0, 255, 0, 50, false, 20};
  sls::Look minimum{sls::ColorMode::HueSat, 0, 255, 0, 10, false, 20};
  sls::Frame fh = sls::render(half, true, 1, 10000);
  sls::Frame fm = sls::render(minimum, true, 1, 10000);
  TEST_ASSERT_EQUAL_UINT8(127, fh.r);
  TEST_ASSERT_EQUAL_UINT8(0, fh.g);
  TEST_ASSERT_EQUAL_UINT8(0, fh.b);
  TEST_ASSERT_EQUAL_UINT8(25, fm.r);
}

void test_warm_white_has_more_red_than_blue() {
  sls::Look warm{sls::ColorMode::ColorTemp, 0, 0, 500, 100, false, 20};
  sls::Frame f = sls::render(warm, true, 1, 10000);
  TEST_ASSERT_GREATER_THAN_UINT8(f.b, f.r);
}

void test_cool_white_has_more_blue_than_warm_white() {
  sls::Look warm{sls::ColorMode::ColorTemp, 0, 0, 500, 100, false, 20};
  sls::Look cool{sls::ColorMode::ColorTemp, 0, 0, 153, 100, false, 20};
  sls::Frame fw = sls::render(warm, true, 1, 10000);
  sls::Frame fc = sls::render(cool, true, 1, 10000);
  TEST_ASSERT_GREATER_THAN_UINT8(fw.b, fc.b);
}

static uint32_t estimated_milliamps(const sls::Frame& f, uint16_t led_count) {
  return (static_cast<uint32_t>(f.r) + f.g + f.b) * 20u * led_count / 255u;
}

void test_power_cap_scales_bright_white_down_to_budget() {
  sls::Look white{sls::ColorMode::HueSat, 0, 0, 0, 100, false, 20};
  sls::Frame f = sls::render(white, true, 60, 2000);
  TEST_ASSERT_LESS_THAN_UINT8(255, f.scale);
  TEST_ASSERT_TRUE(estimated_milliamps(f, 60) <= 2000);
  TEST_ASSERT_GREATER_THAN_UINT8(0, f.r);
  TEST_ASSERT_GREATER_THAN_UINT8(0, f.g);
  TEST_ASSERT_GREATER_THAN_UINT8(0, f.b);
}

void test_power_cap_leaves_frame_untouched_when_under_budget() {
  sls::Look red{sls::ColorMode::HueSat, 0, 255, 0, 30, false, 20};
  sls::Frame capped = sls::render(red, true, 60, 2000);
  sls::Frame uncapped = sls::render(red, true, 60, 65535);
  TEST_ASSERT_EQUAL_UINT8(255, capped.scale);
  TEST_ASSERT_EQUAL_UINT8(uncapped.r, capped.r);
  TEST_ASSERT_EQUAL_UINT8(uncapped.g, capped.g);
  TEST_ASSERT_EQUAL_UINT8(uncapped.b, capped.b);
}

void test_set_color_temp_switches_to_color_temp_and_keeps_the_rest_of_the_look() {
  sls::GroupState state = sls::default_group_state();
  state = sls::apply(state, sls::Command::set_hue_sat(120, 80));
  state = sls::apply(state, sls::Command::set_level(60));
  sls::GroupState before = state;
  before.open = false;
  sls::GroupState after = sls::apply(before, sls::Command::set_color_temp(300));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(sls::ColorMode::ColorTemp),
                          static_cast<uint8_t>(after.look.color_mode));
  TEST_ASSERT_EQUAL_UINT16(300, after.look.color_temp_mireds);
  TEST_ASSERT_EQUAL_UINT8(before.look.brightness_pct, after.look.brightness_pct);
  TEST_ASSERT_EQUAL_UINT16(before.look.hue_deg, after.look.hue_deg);
  TEST_ASSERT_EQUAL_UINT8(before.look.saturation, after.look.saturation);
  TEST_ASSERT_EQUAL(before.look.blink_on, after.look.blink_on);
  TEST_ASSERT_EQUAL(before.motion_enabled, after.motion_enabled);
  TEST_ASSERT_TRUE(after.open);
}

void test_last_color_command_wins() {
  sls::GroupState state = sls::default_group_state();
  state = sls::apply(state, sls::Command::set_color_temp(300));
  state = sls::apply(state, sls::Command::set_hue_sat(200, 90));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(sls::ColorMode::HueSat),
                          static_cast<uint8_t>(state.look.color_mode));
  state = sls::apply(state, sls::Command::set_color_temp(400));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(sls::ColorMode::ColorTemp),
                          static_cast<uint8_t>(state.look.color_mode));
  TEST_ASSERT_EQUAL_UINT16(200, state.look.hue_deg);
  TEST_ASSERT_EQUAL_UINT8(90, state.look.saturation);
  TEST_ASSERT_EQUAL_UINT16(400, state.look.color_temp_mireds);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_brightness_scales_channels_linearly_and_keeps_minimum_visible);
  RUN_TEST(test_open_full_brightness_full_saturation_renders_pure_primaries);
  RUN_TEST(test_closed_strip_renders_black);
  RUN_TEST(test_warm_white_has_more_red_than_blue);
  RUN_TEST(test_cool_white_has_more_blue_than_warm_white);
  RUN_TEST(test_power_cap_scales_bright_white_down_to_budget);
  RUN_TEST(test_power_cap_leaves_frame_untouched_when_under_budget);
  RUN_TEST(test_set_color_temp_switches_to_color_temp_and_keeps_the_rest_of_the_look);
  RUN_TEST(test_last_color_command_wins);
  return UNITY_END();
}
