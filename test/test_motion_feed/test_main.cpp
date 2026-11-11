#include <unity.h>
#include "motion_feed.h"

void setUp() {}
void tearDown() {}

static const uint32_t ABSENCE_MS = 60000;

void test_first_presence_opens_strip_while_motion_enabled() {
  sls::MotionFeed feed;
  TEST_ASSERT_EQUAL(sls::MotionIntent::Open, feed.feed(true, true, 1000));
}

void test_presence_after_seeding_does_not_open_strip() {
  sls::MotionFeed feed;
  feed.feed(true, std::nullopt, 0);
  feed.seed_next();
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, feed.feed(true, true, 1000));
}

void test_arrival_after_seeded_presence_opens_strip() {
  sls::MotionFeed feed;
  feed.feed(true, std::nullopt, 0);
  feed.seed_next();
  feed.feed(true, true, 1000);
  feed.feed(true, false, 2000);
  TEST_ASSERT_EQUAL(sls::MotionIntent::Open, feed.feed(true, true, 3000));
}

void test_seeded_absence_closes_strip_after_timeout() {
  sls::MotionFeed feed;
  feed.feed(true, std::nullopt, 0);
  feed.seed_next();
  feed.feed(true, false, 1000);
  TEST_ASSERT_EQUAL(sls::MotionIntent::Close, feed.feed(true, false, 1000 + ABSENCE_MS));
}

void test_repeated_presence_does_not_open_again() {
  sls::MotionFeed feed;
  feed.feed(true, true, 1000);
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, feed.feed(true, true, 2000));
}

void test_repeated_presence_after_manual_close_does_not_reopen() {
  sls::MotionFeed feed;
  feed.feed(true, true, 1000);
  feed.on_manual_close();
  TEST_ASSERT_EQUAL(sls::MotionIntent::None, feed.feed(true, true, 2000));
}

void test_enabling_motion_with_room_already_empty_closes_after_timeout() {
  sls::MotionFeed feed;
  feed.feed(false, false, 0);
  feed.feed(true, false, 1000);
  TEST_ASSERT_EQUAL(sls::MotionIntent::Close, feed.feed(true, false, 1000 + ABSENCE_MS));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_first_presence_opens_strip_while_motion_enabled);
  RUN_TEST(test_presence_after_seeding_does_not_open_strip);
  RUN_TEST(test_arrival_after_seeded_presence_opens_strip);
  RUN_TEST(test_seeded_absence_closes_strip_after_timeout);
  RUN_TEST(test_repeated_presence_does_not_open_again);
  RUN_TEST(test_repeated_presence_after_manual_close_does_not_reopen);
  RUN_TEST(test_enabling_motion_with_room_already_empty_closes_after_timeout);
  return UNITY_END();
}
