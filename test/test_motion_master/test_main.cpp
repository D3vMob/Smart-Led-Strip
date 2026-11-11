#include <unity.h>
#include "motion_master.h"

void setUp() {}
void tearDown() {}

static const sls::Mac kSensorA = {0xAA, 0, 0, 0, 0, 0x01};
static const sls::Mac kLow = {0x10, 0, 0, 0, 0, 0x01};
static const sls::Mac kHigh = {0x20, 0, 0, 0, 0, 0x01};

void test_group_with_one_sensor_uses_it_as_master() {
  sls::MotionMaster mm;
  mm.on_presence_report(kSensorA, false, 0, 0);
  TEST_ASSERT_TRUE(mm.master().has_value());
  TEST_ASSERT_TRUE(kSensorA == *mm.master());
  TEST_ASSERT_TRUE(mm.motion_presence().has_value());
  TEST_ASSERT_FALSE(*mm.motion_presence());
}

void test_group_without_sensor_has_no_master() {
  sls::MotionMaster mm;
  TEST_ASSERT_FALSE(mm.master().has_value());
}

void test_before_calibration_ends_lowest_mac_heard_is_master_in_any_report_order() {
  sls::MotionMaster high_first;
  high_first.on_presence_report(kHigh, false, 0, 0);
  high_first.on_presence_report(kLow, false, 0, 1);
  TEST_ASSERT_TRUE(kLow == *high_first.master());

  sls::MotionMaster low_first;
  low_first.on_presence_report(kLow, false, 0, 0);
  low_first.on_presence_report(kHigh, false, 0, 1);
  TEST_ASSERT_TRUE(kLow == *low_first.master());
  TEST_ASSERT_TRUE(low_first.calibrating());
}

void test_calibration_ends_after_ten_onsets_and_first_reporter_becomes_master() {
  sls::MotionMaster mm;
  uint32_t now = 0;
  for (int round = 0; round < 10; ++round) {
    const sls::Mac& first = round < 3 ? kLow : kHigh;
    const sls::Mac& second = round < 3 ? kHigh : kLow;
    mm.on_presence_report(first, true, 0, now++);
    mm.on_presence_report(second, true, 0, now++);
    mm.on_presence_report(kLow, false, 0, now++);
    mm.on_presence_report(kHigh, false, 0, now++);
  }
  TEST_ASSERT_FALSE(mm.calibrating());
  TEST_ASSERT_TRUE(kHigh == *mm.master());
  std::vector<sls::Mac> expected = {kHigh, kLow};
  TEST_ASSERT_TRUE(expected == mm.ranking());
}

void test_keepalives_and_late_second_onset_do_not_count_as_onsets() {
  sls::MotionMaster mm;
  mm.on_presence_report(kHigh, true, 0, 0);
  mm.on_presence_report(kLow, true, 0, 1);
  for (uint32_t i = 0; i < 9; ++i) mm.on_presence_report(kHigh, true, 0, 2 + i);
  TEST_ASSERT_TRUE(mm.calibrating());
  std::vector<sls::Mac> expected = {kHigh, kLow};
  TEST_ASSERT_TRUE(expected == mm.ranking());
}

// One empty-to-present round: `first` reports present, `second` joins, then both leave.
static void run_round(sls::MotionMaster& mm, const sls::Mac& first, const sls::Mac& second,
                      uint32_t low_total, uint32_t high_total, uint32_t& now) {
  auto total = [&](const sls::Mac& m) { return m == kLow ? low_total : high_total; };
  mm.on_presence_report(first, true, total(first), now++);
  mm.on_presence_report(second, true, total(second), now++);
  mm.on_presence_report(kLow, false, low_total, now++);
  mm.on_presence_report(kHigh, false, high_total, now++);
}

void test_equal_points_are_broken_by_longer_total_presence() {
  sls::MotionMaster mm;
  uint32_t now = 0;
  for (int i = 0; i < 5; ++i) run_round(mm, kLow, kHigh, 100, 500, now);
  for (int i = 0; i < 5; ++i) run_round(mm, kHigh, kLow, 100, 500, now);
  TEST_ASSERT_FALSE(mm.calibrating());
  TEST_ASSERT_TRUE(kHigh == *mm.master());
  std::vector<sls::Mac> expected = {kHigh, kLow};
  TEST_ASSERT_TRUE(expected == mm.ranking());
}

void test_equal_points_and_equal_totals_are_broken_by_lowest_mac() {
  sls::MotionMaster mm;
  uint32_t now = 0;
  for (int i = 0; i < 5; ++i) run_round(mm, kHigh, kLow, 300, 300, now);
  for (int i = 0; i < 5; ++i) run_round(mm, kLow, kHigh, 300, 300, now);
  TEST_ASSERT_TRUE(kLow == *mm.master());
  std::vector<sls::Mac> expected = {kLow, kHigh};
  TEST_ASSERT_TRUE(expected == mm.ranking());
}

void test_ranking_is_frozen_once_calibration_ends() {
  sls::MotionMaster mm;
  uint32_t now = 0;
  for (int i = 0; i < 5; ++i) run_round(mm, kLow, kHigh, 100, 500, now);
  for (int i = 0; i < 5; ++i) run_round(mm, kHigh, kLow, 100, 500, now);
  for (int i = 0; i < 10; ++i) run_round(mm, kLow, kHigh, 9000, 500, now);
  std::vector<sls::Mac> expected = {kHigh, kLow};
  TEST_ASSERT_TRUE(kHigh == *mm.master());
  TEST_ASSERT_TRUE(expected == mm.ranking());
}

void test_sensor_first_heard_after_calibration_goes_to_end_of_ranking() {
  static const sls::Mac kNew = {0x05, 0, 0, 0, 0, 0x01};
  sls::MotionMaster mm;
  uint32_t now = 0;
  for (int i = 0; i < 5; ++i) run_round(mm, kLow, kHigh, 100, 500, now);
  for (int i = 0; i < 5; ++i) run_round(mm, kHigh, kLow, 100, 500, now);
  mm.on_presence_report(kNew, true, 99999, now++);
  std::vector<sls::Mac> expected = {kHigh, kLow, kNew};
  TEST_ASSERT_TRUE(expected == mm.ranking());
}

// A (low MAC) is master; B keeps reporting every 2 s while A goes quiet.
static void a_goes_quiet_while_b_keeps_reporting(sls::MotionMaster& mm) {
  mm.on_presence_report(kLow, false, 0, 0);
  mm.on_presence_report(kHigh, true, 0, 0);
  for (uint32_t t = 2000; t <= 10000; t += 2000) mm.on_presence_report(kHigh, true, 0, t);
}

void test_master_silent_for_more_than_10_s_fails_over_to_next_sensor() {
  sls::MotionMaster mm;
  a_goes_quiet_while_b_keeps_reporting(mm);
  mm.on_tick(9900);
  TEST_ASSERT_TRUE(kLow == *mm.master());
  mm.on_tick(10001);
  TEST_ASSERT_TRUE(kHigh == *mm.master());
}

void test_silent_master_takes_its_place_back_when_it_reports_again() {
  sls::MotionMaster mm;
  a_goes_quiet_while_b_keeps_reporting(mm);
  mm.on_tick(10001);
  mm.on_presence_report(kLow, false, 0, 10002);
  TEST_ASSERT_TRUE(kLow == *mm.master());
}

void test_all_sensors_silent_leaves_no_master() {
  sls::MotionMaster mm;
  mm.on_presence_report(kLow, true, 0, 0);
  mm.on_presence_report(kHigh, false, 0, 0);
  mm.on_tick(10001);
  TEST_ASSERT_FALSE(mm.master().has_value());
}

void test_motion_presence_follows_the_new_master_after_failover() {
  sls::MotionMaster mm;
  a_goes_quiet_while_b_keeps_reporting(mm);
  mm.on_tick(10001);
  TEST_ASSERT_TRUE(mm.motion_presence().has_value());
  TEST_ASSERT_TRUE(*mm.motion_presence());
}

void test_failover_timing_survives_the_millisecond_clock_wrapping() {
  sls::MotionMaster mm;
  mm.on_presence_report(kLow, false, 0, 0xFFFFF000u);
  mm.on_presence_report(kHigh, false, 0, 0xFFFFF000u);
  mm.on_presence_report(kHigh, false, 0, 0x00000400u);
  mm.on_presence_report(kHigh, false, 0, 0x00001400u);
  mm.on_tick(5900);
  TEST_ASSERT_TRUE(kLow == *mm.master());
  mm.on_tick(5905);
  TEST_ASSERT_TRUE(kHigh == *mm.master());
}

void test_override_makes_a_sensor_master_and_ends_calibration() {
  sls::MotionMaster mm;
  mm.on_presence_report(kLow, false, 0, 0);
  mm.on_presence_report(kHigh, false, 0, 0);
  mm.on_override(kHigh, 1);
  TEST_ASSERT_TRUE(kHigh == *mm.master());
  TEST_ASSERT_FALSE(mm.calibrating());
}

void test_onsets_after_an_override_do_not_change_the_master() {
  sls::MotionMaster mm;
  mm.on_presence_report(kLow, false, 0, 0);
  mm.on_presence_report(kHigh, false, 0, 0);
  mm.on_override(kHigh, 1);
  uint32_t now = 2;
  for (int i = 0; i < 10; ++i) run_round(mm, kLow, kHigh, 0, 0, now);
  TEST_ASSERT_TRUE(kHigh == *mm.master());
  TEST_ASSERT_FALSE(mm.calibrating());
}

void test_override_wins_over_a_finished_calibration() {
  sls::MotionMaster mm;
  uint32_t now = 0;
  for (int i = 0; i < 10; ++i) run_round(mm, kLow, kHigh, 0, 0, now);
  TEST_ASSERT_TRUE(kLow == *mm.master());
  mm.on_override(kHigh, now);
  TEST_ASSERT_TRUE(kHigh == *mm.master());
}

void test_a_later_override_replaces_the_earlier_one() {
  sls::MotionMaster mm;
  mm.on_presence_report(kLow, false, 0, 0);
  mm.on_override(kHigh, 1);
  mm.on_override(kLow, 2);
  TEST_ASSERT_TRUE(kLow == *mm.master());
}

void test_silent_override_stays_master_with_its_last_presence() {
  sls::MotionMaster mm;
  mm.on_presence_report(kLow, false, 0, 0);
  mm.on_override(kHigh, 0);
  mm.on_presence_report(kHigh, true, 0, 1);
  mm.on_presence_report(kLow, false, 0, 29999);
  mm.on_tick(30000);
  TEST_ASSERT_TRUE(kHigh == *mm.master());
  TEST_ASSERT_TRUE(*mm.motion_presence());
}

void test_a_later_override_replaces_a_silent_override() {
  sls::MotionMaster mm;
  mm.on_presence_report(kLow, false, 0, 0);
  mm.on_override(kHigh, 0);
  mm.on_presence_report(kLow, false, 0, 29999);
  mm.on_tick(30000);
  mm.on_override(kLow, 30000);
  TEST_ASSERT_TRUE(kLow == *mm.master());
}

void test_override_ranks_first_and_is_recorded() {
  sls::MotionMaster mm;
  mm.on_presence_report(kLow, false, 0, 0);
  mm.on_override(kHigh, 1);
  std::vector<sls::Mac> expected = {kHigh, kLow};
  TEST_ASSERT_TRUE(expected == mm.ranking());
  TEST_ASSERT_TRUE(mm.record().override_mac.has_value());
  TEST_ASSERT_TRUE(kHigh == *mm.record().override_mac);
}

void test_record_lists_scores_in_ranking_order() {
  sls::MotionMaster mm;
  uint32_t now = 0;
  for (int i = 0; i < 6; ++i) run_round(mm, kHigh, kLow, 100, 500, now);
  for (int i = 0; i < 4; ++i) run_round(mm, kLow, kHigh, 100, 500, now);
  const sls::MotionMasterRecord rec = mm.record();
  TEST_ASSERT_EQUAL(10, rec.onsets_counted);
  TEST_ASSERT_FALSE(rec.override_mac.has_value());
  TEST_ASSERT_EQUAL(2, rec.scores.size());
  TEST_ASSERT_TRUE(kHigh == rec.scores[0].mac);
  TEST_ASSERT_EQUAL(6, rec.scores[0].points);
  TEST_ASSERT_EQUAL(500, rec.scores[0].total_presence_ms);
  TEST_ASSERT_TRUE(kLow == rec.scores[1].mac);
  TEST_ASSERT_EQUAL(4, rec.scores[1].points);
  TEST_ASSERT_EQUAL(100, rec.scores[1].total_presence_ms);
}

void test_restored_ranking_keeps_the_saved_order_after_calibration() {
  sls::MotionMasterRecord saved;
  saved.onsets_counted = 10;
  saved.scores = {{kHigh, 1, 0}, {kLow, 9, 0}};
  sls::MotionMaster mm(saved);
  TEST_ASSERT_FALSE(mm.calibrating());
  std::vector<sls::Mac> expected = {kHigh, kLow};
  TEST_ASSERT_TRUE(expected == mm.ranking());
}

void test_restored_calibration_keeps_counting_saved_points() {
  sls::MotionMasterRecord saved;
  saved.onsets_counted = 9;
  saved.scores = {{kHigh, 6, 0}, {kLow, 3, 0}};
  sls::MotionMaster mm(saved);
  TEST_ASSERT_TRUE(mm.calibrating());
  mm.on_presence_report(kLow, true, 0, 0);
  TEST_ASSERT_FALSE(mm.calibrating());
  std::vector<sls::Mac> expected = {kHigh, kLow};
  TEST_ASSERT_TRUE(expected == mm.ranking());
}

void test_restored_sensor_is_never_master_until_it_reports() {
  sls::MotionMasterRecord saved;
  saved.override_mac = kHigh;
  saved.onsets_counted = 10;
  saved.scores = {{kHigh, 5, 0}, {kLow, 5, 0}};
  sls::MotionMaster mm(saved);
  TEST_ASSERT_FALSE(mm.master().has_value());
  TEST_ASSERT_EQUAL(2, mm.ranking().size());
  mm.on_presence_report(kLow, false, 0, 100);
  TEST_ASSERT_TRUE(kLow == *mm.master());
  mm.on_presence_report(kHigh, false, 0, 200);
  TEST_ASSERT_TRUE(kHigh == *mm.master());
}

static sls::MotionMasterRecord record_with(size_t scores, bool with_override) {
  sls::MotionMasterRecord r;
  r.onsets_counted = 7;
  if (with_override) r.override_mac = kHigh;
  for (size_t i = 0; i < scores; ++i) {
    r.scores.push_back({{0x10, 0, 0, 0, 0, static_cast<uint8_t>(i)}, static_cast<uint16_t>(300 + i),
                        static_cast<uint32_t>(70000 + i)});
  }
  return r;
}

static void assert_round_trips(const sls::MotionMasterRecord& in) {
  const std::vector<uint8_t> bytes = sls::encode_motion_master_record(in);
  auto out = sls::decode_motion_master_record(bytes.data(), bytes.size());
  TEST_ASSERT_TRUE(out.has_value());
  TEST_ASSERT_EQUAL(in.onsets_counted, out->onsets_counted);
  TEST_ASSERT_EQUAL(in.override_mac.has_value(), out->override_mac.has_value());
  if (in.override_mac) TEST_ASSERT_TRUE(*in.override_mac == *out->override_mac);
  TEST_ASSERT_EQUAL(in.scores.size(), out->scores.size());
  for (size_t i = 0; i < in.scores.size(); ++i) {
    TEST_ASSERT_TRUE(in.scores[i].mac == out->scores[i].mac);
    TEST_ASSERT_EQUAL(in.scores[i].points, out->scores[i].points);
    TEST_ASSERT_EQUAL(in.scores[i].total_presence_ms, out->scores[i].total_presence_ms);
  }
}

void test_record_round_trips_through_its_byte_form() {
  assert_round_trips(record_with(0, false));
  assert_round_trips(record_with(0, true));
  assert_round_trips(record_with(8, false));
  assert_round_trips(record_with(8, true));
}

void test_decode_rejects_wrong_version() {
  std::vector<uint8_t> bytes = sls::encode_motion_master_record(record_with(2, true));
  bytes[0] ^= 0xFF;
  TEST_ASSERT_FALSE(sls::decode_motion_master_record(bytes.data(), bytes.size()).has_value());
}

void test_decode_rejects_truncated_or_extra_bytes() {
  std::vector<uint8_t> bytes = sls::encode_motion_master_record(record_with(2, true));
  TEST_ASSERT_FALSE(sls::decode_motion_master_record(bytes.data(), bytes.size() - 1).has_value());
  TEST_ASSERT_FALSE(sls::decode_motion_master_record(bytes.data(), 0).has_value());
  bytes.push_back(0);
  TEST_ASSERT_FALSE(sls::decode_motion_master_record(bytes.data(), bytes.size()).has_value());
}

void test_decode_rejects_more_than_eight_scores() {
  const std::vector<uint8_t> bytes = sls::encode_motion_master_record(record_with(9, false));
  TEST_ASSERT_FALSE(sls::decode_motion_master_record(bytes.data(), bytes.size()).has_value());
}

void test_sensor_gone_silent_while_present_does_not_block_later_onsets() {
  sls::MotionMaster mm;
  mm.on_presence_report(kLow, true, 0, 0);
  mm.on_presence_report(kHigh, false, 0, 0);
  mm.on_presence_report(kHigh, false, 0, 5000);
  mm.on_presence_report(kHigh, true, 0, 11000);
  TEST_ASSERT_EQUAL(2, mm.record().onsets_counted);
}

void test_motion_presence_is_unknown_with_no_sensor_known() {
  sls::MotionMaster mm;
  mm.on_tick(100000);
  TEST_ASSERT_FALSE(mm.motion_presence().has_value());
}

void test_motion_presence_is_absent_when_every_sensor_goes_silent() {
  sls::MotionMaster mm;
  mm.on_presence_report(kLow, true, 0, 0);
  mm.on_tick(10001);
  TEST_ASSERT_TRUE(mm.motion_presence().has_value());
  TEST_ASSERT_FALSE(*mm.motion_presence());
}

void test_motion_presence_follows_the_master() {
  sls::MotionMaster mm;
  mm.on_presence_report(kLow, true, 0, 0);
  TEST_ASSERT_TRUE(mm.motion_presence().has_value());
  TEST_ASSERT_TRUE(*mm.motion_presence());
}

void test_silent_override_keeps_its_last_presence_as_motion_presence() {
  sls::MotionMaster mm;
  mm.on_override(kHigh, 0);
  mm.on_presence_report(kHigh, true, 0, 1);
  mm.on_tick(30000);
  TEST_ASSERT_TRUE(mm.motion_presence().has_value());
  TEST_ASSERT_TRUE(*mm.motion_presence());
}

void test_motion_presence_is_absent_before_restored_sensors_report() {
  sls::MotionMasterRecord saved;
  saved.scores.push_back({kLow, 3, 100});
  sls::MotionMaster mm(saved);
  TEST_ASSERT_TRUE(mm.motion_presence().has_value());
  TEST_ASSERT_FALSE(*mm.motion_presence());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_group_with_one_sensor_uses_it_as_master);
  RUN_TEST(test_group_without_sensor_has_no_master);
  RUN_TEST(test_before_calibration_ends_lowest_mac_heard_is_master_in_any_report_order);
  RUN_TEST(test_calibration_ends_after_ten_onsets_and_first_reporter_becomes_master);
  RUN_TEST(test_keepalives_and_late_second_onset_do_not_count_as_onsets);
  RUN_TEST(test_equal_points_are_broken_by_longer_total_presence);
  RUN_TEST(test_equal_points_and_equal_totals_are_broken_by_lowest_mac);
  RUN_TEST(test_ranking_is_frozen_once_calibration_ends);
  RUN_TEST(test_sensor_first_heard_after_calibration_goes_to_end_of_ranking);
  RUN_TEST(test_master_silent_for_more_than_10_s_fails_over_to_next_sensor);
  RUN_TEST(test_silent_master_takes_its_place_back_when_it_reports_again);
  RUN_TEST(test_all_sensors_silent_leaves_no_master);
  RUN_TEST(test_motion_presence_follows_the_new_master_after_failover);
  RUN_TEST(test_failover_timing_survives_the_millisecond_clock_wrapping);
  RUN_TEST(test_override_makes_a_sensor_master_and_ends_calibration);
  RUN_TEST(test_onsets_after_an_override_do_not_change_the_master);
  RUN_TEST(test_override_wins_over_a_finished_calibration);
  RUN_TEST(test_a_later_override_replaces_the_earlier_one);
  RUN_TEST(test_silent_override_stays_master_with_its_last_presence);
  RUN_TEST(test_a_later_override_replaces_a_silent_override);
  RUN_TEST(test_motion_presence_is_unknown_with_no_sensor_known);
  RUN_TEST(test_motion_presence_is_absent_when_every_sensor_goes_silent);
  RUN_TEST(test_motion_presence_follows_the_master);
  RUN_TEST(test_silent_override_keeps_its_last_presence_as_motion_presence);
  RUN_TEST(test_motion_presence_is_absent_before_restored_sensors_report);
  RUN_TEST(test_override_ranks_first_and_is_recorded);
  RUN_TEST(test_record_lists_scores_in_ranking_order);
  RUN_TEST(test_restored_ranking_keeps_the_saved_order_after_calibration);
  RUN_TEST(test_restored_calibration_keeps_counting_saved_points);
  RUN_TEST(test_restored_sensor_is_never_master_until_it_reports);
  RUN_TEST(test_record_round_trips_through_its_byte_form);
  RUN_TEST(test_decode_rejects_wrong_version);
  RUN_TEST(test_decode_rejects_truncated_or_extra_bytes);
  RUN_TEST(test_decode_rejects_more_than_eight_scores);
  RUN_TEST(test_sensor_gone_silent_while_present_does_not_block_later_onsets);
  return UNITY_END();
}
