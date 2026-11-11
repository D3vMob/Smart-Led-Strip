#include <unity.h>
#include "blink.h"
#include "group_state.h"

void setUp() {}
void tearDown() {}

void test_speed_one_percent_is_50_ms() {
  TEST_ASSERT_EQUAL_UINT32(50, sls::blink_period_ms(1));
}

void test_default_speed_twenty_percent_is_1000_ms() {
  TEST_ASSERT_EQUAL_UINT32(1000, sls::blink_period_ms(20));
}

void test_speed_hundred_percent_is_5000_ms() {
  TEST_ASSERT_EQUAL_UINT32(5000, sls::blink_period_ms(100));
}

void test_speed_zero_is_raised_to_one_percent() {
  TEST_ASSERT_EQUAL_UINT32(50, sls::blink_period_ms(0));
}

void test_speed_above_hundred_is_capped() {
  TEST_ASSERT_EQUAL_UINT32(5000, sls::blink_period_ms(150));
  TEST_ASSERT_EQUAL_UINT32(5000, sls::blink_period_ms(255));
}

static sls::Look blinking_look(uint8_t speed_pct) {
  sls::Look look = sls::default_group_state().look;
  look.blink_on = true;
  look.blink_speed_pct = speed_pct;
  return look;
}

void test_closed_strip_is_never_lit() {
  sls::Look steady = sls::default_group_state().look;
  steady.blink_on = false;
  TEST_ASSERT_FALSE(sls::blink_lit(steady, false, 0));
  TEST_ASSERT_FALSE(sls::blink_lit(blinking_look(20), false, 0));
}

void test_open_strip_without_blink_is_always_lit() {
  sls::Look steady = sls::default_group_state().look;
  steady.blink_on = false;
  TEST_ASSERT_TRUE(sls::blink_lit(steady, true, 0));
  TEST_ASSERT_TRUE(sls::blink_lit(steady, true, 777));
}

void test_blinking_strip_is_lit_in_first_half_of_period() {
  TEST_ASSERT_TRUE(sls::blink_lit(blinking_look(20), true, 0));
  TEST_ASSERT_TRUE(sls::blink_lit(blinking_look(20), true, 499));
}

void test_blinking_strip_is_dark_in_second_half_of_period() {
  TEST_ASSERT_FALSE(sls::blink_lit(blinking_look(20), true, 500));
  TEST_ASSERT_FALSE(sls::blink_lit(blinking_look(20), true, 999));
}

void test_blink_cycle_repeats_every_period() {
  TEST_ASSERT_TRUE(sls::blink_lit(blinking_look(20), true, 1000));
  TEST_ASSERT_FALSE(sls::blink_lit(blinking_look(20), true, 1500));
}

void test_blink_period_follows_the_look_speed() {
  TEST_ASSERT_TRUE(sls::blink_lit(blinking_look(1), true, 24));
  TEST_ASSERT_FALSE(sls::blink_lit(blinking_look(1), true, 25));
}

void test_fresh_clock_counts_from_time_zero() {
  sls::BlinkClock clock;
  TEST_ASSERT_EQUAL_UINT32(0, clock.phase_ms(0, 1000));
  TEST_ASSERT_EQUAL_UINT32(250, clock.phase_ms(250, 1000));
  TEST_ASSERT_EQUAL_UINT32(250, clock.phase_ms(1250, 1000));
}

void test_aligned_clock_follows_the_given_phase_and_wraps() {
  sls::BlinkClock clock;
  clock.align(5000, 300);
  TEST_ASSERT_EQUAL_UINT32(300, clock.phase_ms(5000, 1000));
  TEST_ASSERT_EQUAL_UINT32(500, clock.phase_ms(5200, 1000));
  TEST_ASSERT_EQUAL_UINT32(0, clock.phase_ms(5700, 1000));
}

void test_realigning_replaces_the_earlier_alignment() {
  sls::BlinkClock clock;
  clock.align(5000, 300);
  clock.align(6000, 100);
  TEST_ASSERT_EQUAL_UINT32(100, clock.phase_ms(6000, 1000));
  TEST_ASSERT_EQUAL_UINT32(350, clock.phase_ms(6250, 1000));
}

void test_clock_works_across_millis_wraparound() {
  sls::BlinkClock clock;
  clock.align(0xFFFFFF00u, 0);
  const uint32_t later = 0xFFFFFF00u + 600u;
  TEST_ASSERT_EQUAL_UINT32(600, clock.phase_ms(later, 1000));
}

void test_phase_stays_within_a_changed_period() {
  sls::BlinkClock clock;
  clock.align(5000, 300);
  TEST_ASSERT_TRUE(clock.phase_ms(5777, 50) < 50);
}

void test_blink_on_leaves_a_closed_strip_closed() {
  sls::GroupState state = sls::apply(sls::default_group_state(), sls::Command::set_blink(true));
  TEST_ASSERT_TRUE(state.look.blink_on);
  TEST_ASSERT_FALSE(state.open);
}

void test_blink_on_keeps_an_open_strip_open() {
  sls::GroupState open = sls::apply(sls::default_group_state(), sls::Command::open());
  sls::GroupState state = sls::apply(open, sls::Command::set_blink(true));
  TEST_ASSERT_TRUE(state.look.blink_on);
  TEST_ASSERT_TRUE(state.open);
}

void test_blink_off_turns_blinking_off() {
  sls::GroupState blinking = sls::apply(sls::default_group_state(), sls::Command::set_blink(true));
  sls::GroupState state = sls::apply(blinking, sls::Command::set_blink(false));
  TEST_ASSERT_FALSE(state.look.blink_on);
}

void test_blink_speed_sets_speed_and_leaves_a_closed_strip_closed() {
  sls::GroupState state = sls::apply(sls::default_group_state(), sls::Command::set_blink_speed(70));
  TEST_ASSERT_EQUAL_UINT8(70, state.look.blink_speed_pct);
  TEST_ASSERT_FALSE(state.open);
}

void test_blink_speed_keeps_an_open_strip_open() {
  sls::GroupState open = sls::apply(sls::default_group_state(), sls::Command::open());
  sls::GroupState state = sls::apply(open, sls::Command::set_blink_speed(70));
  TEST_ASSERT_TRUE(state.open);
}

void test_blink_speed_zero_is_raised_to_one_percent() {
  sls::GroupState state = sls::apply(sls::default_group_state(), sls::Command::set_blink_speed(0));
  TEST_ASSERT_EQUAL_UINT8(1, state.look.blink_speed_pct);
}

void test_blink_speed_above_hundred_is_capped() {
  sls::GroupState state = sls::apply(sls::default_group_state(), sls::Command::set_blink_speed(150));
  TEST_ASSERT_EQUAL_UINT8(100, state.look.blink_speed_pct);
}

void test_reopening_a_blinking_strip_keeps_blink_and_speed() {
  sls::GroupState state = sls::default_group_state();
  state = sls::apply(state, sls::Command::set_blink(true));
  state = sls::apply(state, sls::Command::set_blink_speed(70));
  state = sls::apply(state, sls::Command::open());
  state = sls::apply(state, sls::Command::close());
  state = sls::apply(state, sls::Command::open());
  TEST_ASSERT_TRUE(state.look.blink_on);
  TEST_ASSERT_EQUAL_UINT8(70, state.look.blink_speed_pct);
}

void test_level_change_while_blinking_keeps_blink_on() {
  sls::GroupState state = sls::apply(sls::default_group_state(), sls::Command::set_blink(true));
  state = sls::apply(state, sls::Command::set_level(50));
  TEST_ASSERT_TRUE(state.look.blink_on);
}

void test_color_change_while_blinking_keeps_blink_on() {
  sls::GroupState state = sls::apply(sls::default_group_state(), sls::Command::set_blink(true));
  state = sls::apply(state, sls::Command::set_hue_sat(120, 80));
  TEST_ASSERT_TRUE(state.look.blink_on);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_speed_one_percent_is_50_ms);
  RUN_TEST(test_default_speed_twenty_percent_is_1000_ms);
  RUN_TEST(test_speed_hundred_percent_is_5000_ms);
  RUN_TEST(test_speed_zero_is_raised_to_one_percent);
  RUN_TEST(test_speed_above_hundred_is_capped);
  RUN_TEST(test_closed_strip_is_never_lit);
  RUN_TEST(test_open_strip_without_blink_is_always_lit);
  RUN_TEST(test_blinking_strip_is_lit_in_first_half_of_period);
  RUN_TEST(test_blinking_strip_is_dark_in_second_half_of_period);
  RUN_TEST(test_blink_cycle_repeats_every_period);
  RUN_TEST(test_blink_period_follows_the_look_speed);
  RUN_TEST(test_fresh_clock_counts_from_time_zero);
  RUN_TEST(test_aligned_clock_follows_the_given_phase_and_wraps);
  RUN_TEST(test_realigning_replaces_the_earlier_alignment);
  RUN_TEST(test_clock_works_across_millis_wraparound);
  RUN_TEST(test_phase_stays_within_a_changed_period);
  RUN_TEST(test_blink_on_leaves_a_closed_strip_closed);
  RUN_TEST(test_blink_on_keeps_an_open_strip_open);
  RUN_TEST(test_blink_off_turns_blinking_off);
  RUN_TEST(test_blink_speed_sets_speed_and_leaves_a_closed_strip_closed);
  RUN_TEST(test_blink_speed_keeps_an_open_strip_open);
  RUN_TEST(test_blink_speed_zero_is_raised_to_one_percent);
  RUN_TEST(test_blink_speed_above_hundred_is_capped);
  RUN_TEST(test_reopening_a_blinking_strip_keeps_blink_and_speed);
  RUN_TEST(test_level_change_while_blinking_keeps_blink_on);
  RUN_TEST(test_color_change_while_blinking_keeps_blink_on);
  return UNITY_END();
}
