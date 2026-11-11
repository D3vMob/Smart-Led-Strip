#include <unity.h>
#include "button_gesture.h"

void setUp() {}
void tearDown() {}

// Feeds a steady pin level to the button every 10 ms and counts the gestures.
struct Rig {
  sls::ButtonGesture button;
  uint32_t now_ms = 1000;
  int long_presses = 0;
  int double_presses = 0;

  void hold(bool pressed, uint32_t duration_ms) {
    for (uint32_t elapsed = 0; elapsed < duration_ms; elapsed += 10) {
      sls::Gesture gesture = button.update(pressed, now_ms);
      if (gesture == sls::Gesture::LongPress) ++long_presses;
      if (gesture == sls::Gesture::DoublePress) ++double_presses;
      now_ms += 10;
    }
  }
};

void test_long_press_fires_once_while_the_button_is_still_held() {
  Rig rig;
  rig.hold(false, 100);

  rig.hold(true, 1900);
  TEST_ASSERT_EQUAL_INT(0, rig.long_presses);
  rig.hold(true, 200);
  TEST_ASSERT_EQUAL_INT(1, rig.long_presses);
  rig.hold(true, 3000);
  rig.hold(false, 500);

  TEST_ASSERT_EQUAL_INT(1, rig.long_presses);
  TEST_ASSERT_EQUAL_INT(0, rig.double_presses);
}

void test_short_single_press_returns_nothing() {
  Rig rig;
  rig.hold(false, 100);

  rig.hold(true, 200);
  rig.hold(false, 1000);

  TEST_ASSERT_EQUAL_INT(0, rig.long_presses);
  TEST_ASSERT_EQUAL_INT(0, rig.double_presses);
}

void test_two_short_presses_in_quick_succession_fire_a_double_press_on_the_second_release() {
  Rig rig;
  rig.hold(false, 100);

  rig.hold(true, 150);
  rig.hold(false, 200);
  rig.hold(true, 150);
  TEST_ASSERT_EQUAL_INT(0, rig.double_presses);
  rig.hold(false, 500);

  TEST_ASSERT_EQUAL_INT(1, rig.double_presses);
  TEST_ASSERT_EQUAL_INT(0, rig.long_presses);
}

void test_press_started_after_the_gap_is_a_new_first_press_not_a_double_press() {
  Rig rig;
  rig.hold(false, 100);

  rig.hold(true, 150);
  rig.hold(false, 600);
  rig.hold(true, 150);
  rig.hold(false, 200);
  TEST_ASSERT_EQUAL_INT(0, rig.double_presses);

  rig.hold(true, 150);
  rig.hold(false, 500);
  TEST_ASSERT_EQUAL_INT(1, rig.double_presses);
}

void test_bounce_shorter_than_the_debounce_time_is_not_a_press() {
  Rig rig;
  rig.hold(false, 100);

  rig.hold(true, 20);
  rig.hold(false, 100);
  rig.hold(true, 200);
  rig.hold(false, 1000);

  TEST_ASSERT_EQUAL_INT(0, rig.double_presses);
  TEST_ASSERT_EQUAL_INT(0, rig.long_presses);
}

void test_double_press_still_fires_when_every_edge_chatters_for_10_ms() {
  Rig rig;
  rig.hold(false, 100);

  for (int press = 0; press < 2; ++press) {
    rig.hold(true, 10);
    rig.hold(false, 10);
    rig.hold(true, 150);
    rig.hold(false, 10);
    rig.hold(true, 10);
    rig.hold(false, 200);
  }
  rig.hold(false, 500);

  TEST_ASSERT_EQUAL_INT(1, rig.double_presses);
}

void test_press_that_was_a_long_press_is_never_half_of_a_double_press() {
  Rig rig;
  rig.hold(false, 100);

  rig.hold(true, 2300);
  rig.hold(false, 100);
  rig.hold(true, 150);
  rig.hold(false, 1000);

  TEST_ASSERT_EQUAL_INT(1, rig.long_presses);
  TEST_ASSERT_EQUAL_INT(0, rig.double_presses);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_long_press_fires_once_while_the_button_is_still_held);
  RUN_TEST(test_short_single_press_returns_nothing);
  RUN_TEST(test_two_short_presses_in_quick_succession_fire_a_double_press_on_the_second_release);
  RUN_TEST(test_press_started_after_the_gap_is_a_new_first_press_not_a_double_press);
  RUN_TEST(test_bounce_shorter_than_the_debounce_time_is_not_a_press);
  RUN_TEST(test_double_press_still_fires_when_every_edge_chatters_for_10_ms);
  RUN_TEST(test_press_that_was_a_long_press_is_never_half_of_a_double_press);
  return UNITY_END();
}
