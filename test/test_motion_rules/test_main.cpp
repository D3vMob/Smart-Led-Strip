#include <unity.h>
#include "motion_rules.h"

void setUp() {}
void tearDown() {}

void test_presence_opens_strip_while_motion_enabled() {
  sls::MotionRules rules;
  rules.on_motion_enabled(true, 0);
  TEST_ASSERT_EQUAL(sls::MotionIntent::Open, rules.on_presence(true, 1000));
}

void test_presence_does_nothing_while_motion_disabled() {
  sls::MotionRules rules;
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_presence(true, 1000));
}

void test_absence_closes_strip_after_60_s() {
  sls::MotionRules rules;
  rules.on_motion_enabled(true, 0);
  rules.on_presence(true, 1000);
  rules.on_presence(false, 2000);
  TEST_ASSERT_EQUAL(sls::MotionIntent::Close, rules.on_tick(62000));
}

void test_absence_does_not_close_strip_before_60_s() {
  sls::MotionRules rules;
  rules.on_motion_enabled(true, 0);
  rules.on_presence(true, 1000);
  rules.on_presence(false, 2000);
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_tick(61900));
}

void test_absence_close_fires_once() {
  sls::MotionRules rules;
  rules.on_motion_enabled(true, 0);
  rules.on_presence(true, 1000);
  rules.on_presence(false, 2000);
  rules.on_tick(62000);
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_tick(70000));
}

void test_presence_returning_before_60_s_cancels_close() {
  sls::MotionRules rules;
  rules.on_motion_enabled(true, 0);
  rules.on_presence(true, 1000);
  rules.on_presence(false, 2000);
  rules.on_presence(true, 30000);
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_tick(62000));
  rules.on_presence(false, 40000);
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_tick(99999));
  TEST_ASSERT_EQUAL(sls::MotionIntent::Close, rules.on_tick(100000));
}

void test_disabling_motion_stops_pending_close() {
  sls::MotionRules rules;
  rules.on_motion_enabled(true, 0);
  rules.on_presence(true, 1000);
  rules.on_presence(false, 2000);
  rules.on_motion_enabled(false, 30000);
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_tick(62000));
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_tick(200000));
}

void test_enabling_motion_while_room_empty_closes_60_s_after_enabling() {
  sls::MotionRules rules;
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_presence(true, 1000));
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_presence(false, 2000));
  rules.on_motion_enabled(true, 100000);
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_tick(159999));
  TEST_ASSERT_EQUAL(sls::MotionIntent::Close, rules.on_tick(160000));
}

void test_manual_close_while_present_holds_through_continued_presence() {
  sls::MotionRules rules;
  rules.on_motion_enabled(true, 0);
  TEST_ASSERT_EQUAL(sls::MotionIntent::Open, rules.on_presence(true, 1000));
  rules.on_manual_close();
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_presence(true, 5000));
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_presence(false, 10000));
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_presence(true, 20000));
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_tick(30000));
}

void test_hold_clears_after_room_empty_60_s_and_next_presence_opens() {
  sls::MotionRules rules;
  rules.on_motion_enabled(true, 0);
  rules.on_presence(true, 1000);
  rules.on_manual_close();
  rules.on_presence(false, 2000);
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_tick(62000));
  TEST_ASSERT_EQUAL(sls::MotionIntent::Open, rules.on_presence(true, 70000));
}

void test_explicit_dim_or_color_clears_hold() {
  sls::MotionRules rules;
  rules.on_motion_enabled(true, 0);
  rules.on_presence(true, 1000);
  rules.on_manual_close();
  rules.on_explicit_open();
  rules.on_presence(false, 2000);
  TEST_ASSERT_EQUAL(sls::MotionIntent::Close, rules.on_tick(62000));
}

void test_without_sensor_enabling_motion_never_closes_strip() {
  sls::MotionRules rules;
  rules.on_motion_enabled(true, 0);
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_tick(60000));
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_tick(600000));
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_tick(4000000000u));
}

void test_manual_close_while_room_empty_sets_no_hold() {
  sls::MotionRules rules;
  rules.on_motion_enabled(true, 0);
  rules.on_presence(true, 1000);
  rules.on_presence(false, 2000);
  rules.on_manual_close();
  TEST_ASSERT_EQUAL(sls::MotionIntent::Open, rules.on_presence(true, 10000));
}

void test_reenabling_motion_clears_hold() {
  sls::MotionRules rules;
  rules.on_motion_enabled(true, 0);
  rules.on_presence(true, 1000);
  rules.on_manual_close();
  rules.on_motion_enabled(false, 2000);
  rules.on_motion_enabled(true, 3000);
  rules.on_presence(false, 4000);
  TEST_ASSERT_EQUAL(sls::MotionIntent::Close, rules.on_tick(64000));
}

void test_repeated_absent_reports_do_not_restart_absence_count() {
  sls::MotionRules rules;
  rules.on_motion_enabled(true, 0);
  rules.on_presence(true, 1000);
  for (uint32_t t = 2000; t < 62000; t += 1000) {
    rules.on_presence(false, t);
    TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_tick(t));
  }
  rules.on_presence(false, 62000);
  TEST_ASSERT_EQUAL(sls::MotionIntent::Close, rules.on_tick(62000));
}

void test_repeated_absent_reports_after_close_do_not_close_again() {
  sls::MotionRules rules;
  rules.on_motion_enabled(true, 0);
  rules.on_presence(true, 1000);
  for (uint32_t t = 2000; t <= 62000; t += 1000) {
    rules.on_presence(false, t);
    rules.on_tick(t);
  }
  for (uint32_t t = 63000; t <= 150000; t += 1000) {
    rules.on_presence(false, t);
    TEST_ASSERT_EQUAL(sls::MotionIntent::None, rules.on_tick(t));
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_presence_opens_strip_while_motion_enabled);
  RUN_TEST(test_presence_does_nothing_while_motion_disabled);
  RUN_TEST(test_absence_closes_strip_after_60_s);
  RUN_TEST(test_absence_does_not_close_strip_before_60_s);
  RUN_TEST(test_absence_close_fires_once);
  RUN_TEST(test_presence_returning_before_60_s_cancels_close);
  RUN_TEST(test_disabling_motion_stops_pending_close);
  RUN_TEST(test_enabling_motion_while_room_empty_closes_60_s_after_enabling);
  RUN_TEST(test_manual_close_while_present_holds_through_continued_presence);
  RUN_TEST(test_hold_clears_after_room_empty_60_s_and_next_presence_opens);
  RUN_TEST(test_explicit_dim_or_color_clears_hold);
  RUN_TEST(test_without_sensor_enabling_motion_never_closes_strip);
  RUN_TEST(test_manual_close_while_room_empty_sets_no_hold);
  RUN_TEST(test_reenabling_motion_clears_hold);
  RUN_TEST(test_repeated_absent_reports_do_not_restart_absence_count);
  RUN_TEST(test_repeated_absent_reports_after_close_do_not_close_again);
  return UNITY_END();
}
