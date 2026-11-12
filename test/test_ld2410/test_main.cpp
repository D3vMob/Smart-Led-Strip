// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#include <string.h>
#include <unity.h>
#include "ld2410.h"

void setUp() {}
void tearDown() {}

static const uint8_t kBasicReport[] = {
    0xF4, 0xF3, 0xF2, 0xF1, 0x0D, 0x00, 0x02, 0xAA, 0x03, 0x51, 0x00, 0x3C,
    0x2D, 0x00, 0x64, 0x8C, 0x00, 0x55, 0x00, 0xF8, 0xF7, 0xF6, 0xF5};

void test_basic_report_frame_decodes_to_one_report() {
  sls::ld2410::FrameParser parser;
  sls::ld2410::Frame frame = {};
  int completed = 0;
  for (unsigned i = 0; i < sizeof(kBasicReport); i++) {
    bool done = parser.feed(kBasicReport[i], frame);
    if (i + 1 < sizeof(kBasicReport)) TEST_ASSERT_FALSE(done);
    if (done) completed++;
  }
  TEST_ASSERT_EQUAL_INT(1, completed);
  TEST_ASSERT_TRUE(frame.kind == sls::ld2410::Frame::Kind::Report);
  TEST_ASSERT_TRUE(frame.report.target == sls::ld2410::TargetState::MovingAndStill);
  TEST_ASSERT_EQUAL_UINT16(81, frame.report.moving_distance_cm);
  TEST_ASSERT_EQUAL_UINT8(60, frame.report.moving_energy);
  TEST_ASSERT_EQUAL_UINT16(45, frame.report.still_distance_cm);
  TEST_ASSERT_EQUAL_UINT8(100, frame.report.still_energy);
  TEST_ASSERT_EQUAL_UINT16(140, frame.report.detection_distance_cm);
}

static const uint8_t kVersionAck[] = {
    0xFD, 0xFC, 0xFB, 0xFA, 0x0C, 0x00, 0xA0, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x02, 0x01, 0x16, 0x24, 0x06, 0x22, 0x04, 0x03, 0x02, 0x01};

static bool feed_all(sls::ld2410::FrameParser& parser, const uint8_t* bytes, unsigned n,
                     sls::ld2410::Frame& frame, int& completed) {
  completed = 0;
  bool early = false;
  for (unsigned i = 0; i < n; i++) {
    bool done = parser.feed(bytes[i], frame);
    if (done) completed++;
    if (done && i + 1 < n) early = true;
  }
  return !early;
}

static int collect(sls::ld2410::FrameParser& parser, const uint8_t* bytes, unsigned n,
                   sls::ld2410::Frame* frames, int max_frames, int already = 0) {
  int count = already;
  for (unsigned i = 0; i < n; i++) {
    sls::ld2410::Frame f = {};
    if (parser.feed(bytes[i], f) && count < max_frames) frames[count++] = f;
  }
  return count;
}

void test_report_frame_split_across_feeds_decodes_once() {
  sls::ld2410::FrameParser parser;
  sls::ld2410::Frame frames[4] = {};
  int n = collect(parser, kBasicReport, 5, frames, 4);
  n = collect(parser, kBasicReport + 5, 11, frames, 4, n);
  n = collect(parser, kBasicReport + 16, sizeof(kBasicReport) - 16, frames, 4, n);
  TEST_ASSERT_EQUAL_INT(1, n);
  TEST_ASSERT_EQUAL_UINT16(81, frames[0].report.moving_distance_cm);
}

void test_leading_garbage_is_skipped() {
  const uint8_t garbage[] = {0x00, 0xFF, 0xF4, 0xF3, 0x12, 0xFD, 0x55};
  sls::ld2410::FrameParser parser;
  sls::ld2410::Frame frames[4] = {};
  int n = collect(parser, garbage, sizeof(garbage), frames, 4);
  n = collect(parser, kBasicReport, sizeof(kBasicReport), frames, 4, n);
  TEST_ASSERT_EQUAL_INT(1, n);
  TEST_ASSERT_EQUAL_UINT16(140, frames[0].report.detection_distance_cm);
}

void test_ack_frame_decodes_command_and_success() {
  sls::ld2410::FrameParser parser;
  sls::ld2410::Frame frame = {};
  int completed;
  TEST_ASSERT_TRUE(feed_all(parser, kVersionAck, sizeof(kVersionAck), frame, completed));
  TEST_ASSERT_EQUAL_INT(1, completed);
  TEST_ASSERT_TRUE(frame.kind == sls::ld2410::Frame::Kind::Ack);
  TEST_ASSERT_EQUAL_UINT16(0x01A0, frame.ack_command);
  TEST_ASSERT_TRUE(frame.ack_ok);
}

void test_ack_frame_with_failure_status_is_not_ok() {
  uint8_t failed[sizeof(kVersionAck)];
  memcpy(failed, kVersionAck, sizeof(failed));
  failed[8] = 0x01;
  sls::ld2410::FrameParser parser;
  sls::ld2410::Frame frame = {};
  int completed;
  TEST_ASSERT_TRUE(feed_all(parser, failed, sizeof(failed), frame, completed));
  TEST_ASSERT_EQUAL_INT(1, completed);
  TEST_ASSERT_TRUE(frame.kind == sls::ld2410::Frame::Kind::Ack);
  TEST_ASSERT_FALSE(frame.ack_ok);
}

static int decode_after(const uint8_t* bad, unsigned bad_n, sls::ld2410::Frame* frames) {
  sls::ld2410::FrameParser parser;
  int n = collect(parser, bad, bad_n, frames, 4);
  return collect(parser, kBasicReport, sizeof(kBasicReport), frames, 4, n);
}

static void assert_single_good_report(int n, const sls::ld2410::Frame* frames) {
  TEST_ASSERT_EQUAL_INT(1, n);
  TEST_ASSERT_TRUE(frames[0].kind == sls::ld2410::Frame::Kind::Report);
  TEST_ASSERT_EQUAL_UINT16(81, frames[0].report.moving_distance_cm);
}

void test_oversized_length_frame_is_dropped_and_next_frame_decodes() {
  const uint8_t bad[] = {0xF4, 0xF3, 0xF2, 0xF1, 0x7F, 0x00};
  sls::ld2410::Frame frames[4] = {};
  assert_single_good_report(decode_after(bad, sizeof(bad), frames), frames);
}

void test_length_that_wraps_the_frame_size_is_dropped_and_next_frame_decodes() {
  const uint8_t bad[] = {0xF4, 0xF3, 0xF2, 0xF1, 0xF6, 0xFF};
  sls::ld2410::Frame frames[4] = {};
  assert_single_good_report(decode_after(bad, sizeof(bad), frames), frames);
}

void test_wrong_length_frame_is_dropped_and_next_frame_decodes() {
  uint8_t bad[sizeof(kBasicReport)];
  memcpy(bad, kBasicReport, sizeof(bad));
  bad[4] = 0x0C;
  sls::ld2410::Frame frames[4] = {};
  assert_single_good_report(decode_after(bad, sizeof(bad), frames), frames);
}

void test_bad_footer_frame_is_dropped_and_next_frame_decodes() {
  uint8_t bad[sizeof(kBasicReport)];
  memcpy(bad, kBasicReport, sizeof(bad));
  bad[sizeof(bad) - 1] = 0x00;
  sls::ld2410::Frame frames[4] = {};
  assert_single_good_report(decode_after(bad, sizeof(bad), frames), frames);
}

void test_wrong_head_marker_frame_is_dropped() {
  uint8_t bad[sizeof(kBasicReport)];
  memcpy(bad, kBasicReport, sizeof(bad));
  bad[7] = 0x00;
  sls::ld2410::Frame frames[4] = {};
  assert_single_good_report(decode_after(bad, sizeof(bad), frames), frames);
}

void test_wrong_tail_marker_frame_is_dropped() {
  uint8_t bad[sizeof(kBasicReport)];
  memcpy(bad, kBasicReport, sizeof(bad));
  bad[17] = 0x00;
  sls::ld2410::Frame frames[4] = {};
  assert_single_good_report(decode_after(bad, sizeof(bad), frames), frames);
}

void test_truncated_frame_followed_by_full_frame_resyncs_on_inner_header() {
  const uint8_t truncated[] = {0xF4, 0xF3, 0xF2, 0xF1, 0x0D, 0x00, 0x02, 0xAA, 0x03};
  sls::ld2410::Frame frames[4] = {};
  assert_single_good_report(decode_after(truncated, sizeof(truncated), frames), frames);
}

void test_report_then_ack_back_to_back_decode_in_order() {
  uint8_t both[sizeof(kBasicReport) + sizeof(kVersionAck)];
  for (unsigned i = 0; i < sizeof(kBasicReport); i++) both[i] = kBasicReport[i];
  for (unsigned i = 0; i < sizeof(kVersionAck); i++) both[sizeof(kBasicReport) + i] = kVersionAck[i];
  sls::ld2410::FrameParser parser;
  sls::ld2410::Frame frames[4] = {};
  int n = collect(parser, both, sizeof(both), frames, 4);
  TEST_ASSERT_EQUAL_INT(2, n);
  TEST_ASSERT_TRUE(frames[0].kind == sls::ld2410::Frame::Kind::Report);
  TEST_ASSERT_TRUE(frames[1].kind == sls::ld2410::Frame::Kind::Ack);
  TEST_ASSERT_EQUAL_UINT16(0x01A0, frames[1].ack_command);
}

void test_encode_read_firmware_version_command() {
  const uint8_t expected[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00,
                              0xA0, 0x00, 0x04, 0x03, 0x02, 0x01};
  uint8_t out[32] = {};
  size_t n = sls::ld2410::encode_command(sls::ld2410::kCmdReadFirmwareVersion, nullptr, 0, out);
  TEST_ASSERT_EQUAL_UINT32(sizeof(expected), n);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, sizeof(expected));
}

void test_encode_enable_config_command_carries_value() {
  const uint8_t value[] = {0x01, 0x00};
  const uint8_t expected[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF,
                              0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
  uint8_t out[32] = {};
  size_t n = sls::ld2410::encode_command(sls::ld2410::kCmdEnableConfig, value, sizeof(value), out);
  TEST_ASSERT_EQUAL_UINT32(sizeof(expected), n);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, sizeof(expected));
}

using sls::ld2410::SensorDetection;

static sls::ld2410::Report report_with(sls::ld2410::TargetState target) {
  sls::ld2410::Report r = {};
  r.target = target;
  return r;
}

void test_detection_is_probing_before_any_frame_and_timeout() {
  SensorDetection detection(1000);
  detection.start(5000);
  TEST_ASSERT_TRUE(detection.status(5500) == SensorDetection::Status::Probing);
}

void test_frame_before_timeout_means_sensor() {
  SensorDetection detection(1000);
  detection.start(5000);
  detection.on_frame(5200);
  TEST_ASSERT_TRUE(detection.status(5300) == SensorDetection::Status::Sensor);
}

void test_no_frame_by_timeout_means_no_sensor() {
  SensorDetection detection(1000);
  detection.start(5000);
  TEST_ASSERT_TRUE(detection.status(6000) == SensorDetection::Status::NoSensor);
}

void test_detection_is_still_probing_just_before_timeout() {
  SensorDetection detection(1000);
  detection.start(5000);
  TEST_ASSERT_TRUE(detection.status(5999) == SensorDetection::Status::Probing);
}

void test_frame_after_no_sensor_verdict_does_not_change_it() {
  SensorDetection detection(1000);
  detection.start(5000);
  detection.on_frame(6500);
  TEST_ASSERT_TRUE(detection.status(6600) == SensorDetection::Status::NoSensor);
}

void test_sensor_verdict_is_held_after_timeout_elapses() {
  SensorDetection detection(1000);
  detection.start(5000);
  detection.on_frame(5100);
  TEST_ASSERT_TRUE(detection.status(900000) == SensorDetection::Status::Sensor);
}

void test_detection_timeout_is_wrap_safe() {
  SensorDetection detection(1000);
  detection.start(0xFFFFFF00u);
  TEST_ASSERT_TRUE(detection.status(0x10u) == SensorDetection::Status::Probing);
  TEST_ASSERT_TRUE(detection.status(0x400u) == SensorDetection::Status::NoSensor);
}

void test_none_target_is_absent_without_onset() {
  sls::ld2410::PresenceTracker tracker;
  sls::ld2410::Presence p = tracker.on_report(report_with(sls::ld2410::TargetState::None));
  TEST_ASSERT_FALSE(p.present);
  TEST_ASSERT_FALSE(p.onset);
}

void test_first_moving_report_is_present_with_onset() {
  sls::ld2410::PresenceTracker tracker;
  sls::ld2410::Presence p = tracker.on_report(report_with(sls::ld2410::TargetState::Moving));
  TEST_ASSERT_TRUE(p.present);
  TEST_ASSERT_TRUE(p.onset);
}

void test_following_present_report_has_no_onset() {
  sls::ld2410::PresenceTracker tracker;
  tracker.on_report(report_with(sls::ld2410::TargetState::Moving));
  sls::ld2410::Presence p = tracker.on_report(report_with(sls::ld2410::TargetState::Still));
  TEST_ASSERT_TRUE(p.present);
  TEST_ASSERT_FALSE(p.onset);
}

void test_present_to_none_is_absent_without_onset() {
  sls::ld2410::PresenceTracker tracker;
  tracker.on_report(report_with(sls::ld2410::TargetState::MovingAndStill));
  sls::ld2410::Presence p = tracker.on_report(report_with(sls::ld2410::TargetState::None));
  TEST_ASSERT_FALSE(p.present);
  TEST_ASSERT_FALSE(p.onset);
}

void test_presence_after_absence_has_onset_again() {
  sls::ld2410::PresenceTracker tracker;
  tracker.on_report(report_with(sls::ld2410::TargetState::Still));
  tracker.on_report(report_with(sls::ld2410::TargetState::None));
  sls::ld2410::Presence p = tracker.on_report(report_with(sls::ld2410::TargetState::Still));
  TEST_ASSERT_TRUE(p.present);
  TEST_ASSERT_TRUE(p.onset);
}

void test_presence_carries_report_distances_and_energies() {
  sls::ld2410::Report in = report_with(sls::ld2410::TargetState::MovingAndStill);
  in.moving_distance_cm = 81;
  in.moving_energy = 60;
  in.still_distance_cm = 45;
  in.still_energy = 100;
  in.detection_distance_cm = 140;
  sls::ld2410::PresenceTracker tracker;
  sls::ld2410::Presence p = tracker.on_report(in);
  TEST_ASSERT_EQUAL_UINT16(81, p.report.moving_distance_cm);
  TEST_ASSERT_EQUAL_UINT8(60, p.report.moving_energy);
  TEST_ASSERT_EQUAL_UINT16(45, p.report.still_distance_cm);
  TEST_ASSERT_EQUAL_UINT8(100, p.report.still_energy);
  TEST_ASSERT_EQUAL_UINT16(140, p.report.detection_distance_cm);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_basic_report_frame_decodes_to_one_report);
  RUN_TEST(test_report_frame_split_across_feeds_decodes_once);
  RUN_TEST(test_leading_garbage_is_skipped);
  RUN_TEST(test_ack_frame_decodes_command_and_success);
  RUN_TEST(test_ack_frame_with_failure_status_is_not_ok);
  RUN_TEST(test_oversized_length_frame_is_dropped_and_next_frame_decodes);
  RUN_TEST(test_length_that_wraps_the_frame_size_is_dropped_and_next_frame_decodes);
  RUN_TEST(test_wrong_length_frame_is_dropped_and_next_frame_decodes);
  RUN_TEST(test_bad_footer_frame_is_dropped_and_next_frame_decodes);
  RUN_TEST(test_wrong_head_marker_frame_is_dropped);
  RUN_TEST(test_wrong_tail_marker_frame_is_dropped);
  RUN_TEST(test_truncated_frame_followed_by_full_frame_resyncs_on_inner_header);
  RUN_TEST(test_report_then_ack_back_to_back_decode_in_order);
  RUN_TEST(test_encode_read_firmware_version_command);
  RUN_TEST(test_encode_enable_config_command_carries_value);
  RUN_TEST(test_detection_is_probing_before_any_frame_and_timeout);
  RUN_TEST(test_frame_before_timeout_means_sensor);
  RUN_TEST(test_no_frame_by_timeout_means_no_sensor);
  RUN_TEST(test_detection_is_still_probing_just_before_timeout);
  RUN_TEST(test_frame_after_no_sensor_verdict_does_not_change_it);
  RUN_TEST(test_sensor_verdict_is_held_after_timeout_elapses);
  RUN_TEST(test_detection_timeout_is_wrap_safe);
  RUN_TEST(test_none_target_is_absent_without_onset);
  RUN_TEST(test_first_moving_report_is_present_with_onset);
  RUN_TEST(test_following_present_report_has_no_onset);
  RUN_TEST(test_present_to_none_is_absent_without_onset);
  RUN_TEST(test_presence_after_absence_has_onset_again);
  RUN_TEST(test_presence_carries_report_distances_and_energies);
  return UNITY_END();
}
