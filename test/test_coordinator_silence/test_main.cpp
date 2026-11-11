#include <unity.h>
#include <vector>
#include "channel_scan.h"
#include "config.h"
#include "group_sync.h"

void setUp() {}
void tearDown() {}

static const sls::Mac kMacs[3] = {{0x02, 0, 0, 0, 0, 0x01}, {0x02, 0, 0, 0, 0, 0x02}, {0x02, 0, 0, 0, 0, 0x03}};
static const uint32_t kGroupId = 0xABCD;
static const uint32_t kBootCount = 0x1234;
static const sls::GroupSecret kSecret = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                                         17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};

static std::vector<sls::Mac> table() { return std::vector<sls::Mac>(kMacs, kMacs + 3); }

static sls::GroupSync coordinator_unit() {
  return sls::GroupSync(kMacs[0], kGroupId, table(), kSecret, sls::SyncRecord{sls::SyncRole::Coordinator}, kBootCount,
                        sls::default_group_state());
}

static sls::GroupSync member_unit(const sls::Mac& self) {
  return sls::GroupSync(self, kGroupId, table(), kSecret, sls::SyncRecord{sls::SyncRole::Member, kMacs[0], 0}, kBootCount,
                        sls::default_group_state());
}

// The Heartbeat a real Coordinator addresses to the Member at kMacs[1].
static sls::Message heartbeat_from(sls::GroupSync& coordinator, uint32_t now_ms,
                                   std::optional<sls::Mac> master = std::nullopt) {
  for (const sls::Outgoing& o : coordinator.on_tick(now_ms, 6, master))
    if (o.to == kMacs[1]) return o.message;
  return sls::Message{};
}

void test_a_member_is_silent_once_the_coordinator_was_quiet_for_more_than_ten_seconds() {
  sls::GroupSync coordinator = coordinator_unit();
  sls::GroupSync member = member_unit(kMacs[1]);
  TEST_ASSERT_TRUE(member.on_message(heartbeat_from(coordinator, 5000), kMacs[0], 5000));
  TEST_ASSERT_FALSE(member.silent(5000 + sls::kCoordinatorSilenceMs));
  TEST_ASSERT_TRUE(member.silent(5000 + sls::kCoordinatorSilenceMs + 1));
}

void test_a_member_that_never_heard_the_coordinator_counts_silence_from_start() {
  sls::GroupSync member = member_unit(kMacs[1]);
  TEST_ASSERT_FALSE(member.silent(sls::kCoordinatorSilenceMs));
  TEST_ASSERT_TRUE(member.silent(sls::kCoordinatorSilenceMs + 1));
}

void test_a_coordinator_or_undecided_unit_is_never_silent() {
  sls::GroupSync coordinator = coordinator_unit();
  sls::GroupSync undecided(kMacs[2], kGroupId, table(), kSecret, sls::SyncRecord{sls::SyncRole::Undecided}, kBootCount,
                           sls::default_group_state());
  TEST_ASSERT_FALSE(coordinator.silent(1000000));
  TEST_ASSERT_FALSE(undecided.silent(1000000));
}

void test_a_member_reports_the_motion_master_named_in_the_coordinators_heartbeat() {
  sls::GroupSync coordinator = coordinator_unit();
  sls::GroupSync member = member_unit(kMacs[1]);
  TEST_ASSERT_TRUE(member.on_message(heartbeat_from(coordinator, 0, kMacs[2]), kMacs[0], 0));
  TEST_ASSERT_TRUE(member.motion_master().has_value());
  TEST_ASSERT_TRUE(*member.motion_master() == kMacs[2]);
}

void test_a_member_reports_no_motion_master_when_the_heartbeat_names_none() {
  sls::GroupSync coordinator = coordinator_unit();
  sls::GroupSync member = member_unit(kMacs[1]);
  TEST_ASSERT_TRUE(member.on_message(heartbeat_from(coordinator, 0, kMacs[2]), kMacs[0], 0));
  TEST_ASSERT_TRUE(member.on_message(heartbeat_from(coordinator, 2000), kMacs[0], 2000));
  TEST_ASSERT_FALSE(member.motion_master().has_value());
}

void test_a_silent_member_keeps_a_local_change_and_sends_nothing() {
  sls::GroupSync member = member_unit(kMacs[1]);
  const sls::GroupState opened = sls::apply(sls::default_group_state(), sls::Command::open());
  const uint32_t now = sls::kCoordinatorSilenceMs + 1;
  TEST_ASSERT_TRUE(member.silent(now));
  TEST_ASSERT_EQUAL(0, member.on_local_change(opened, now).size());
  TEST_ASSERT_TRUE(member.state().open);
}

void test_a_member_that_hears_its_coordinator_still_ignores_a_local_change() {
  sls::GroupSync coordinator = coordinator_unit();
  sls::GroupSync member = member_unit(kMacs[1]);
  TEST_ASSERT_TRUE(member.on_message(heartbeat_from(coordinator, 5000), kMacs[0], 5000));
  const sls::GroupState opened = sls::apply(sls::default_group_state(), sls::Command::open());
  TEST_ASSERT_EQUAL(0, member.on_local_change(opened, 6000).size());
  TEST_ASSERT_FALSE(member.state().open);
}

void test_a_silent_member_sends_its_presence_to_every_other_group_member() {
  sls::GroupSync member = member_unit(kMacs[1]);
  const uint32_t now = sls::kCoordinatorSilenceMs + 1;
  std::vector<sls::Outgoing> out = member.report_presence(true, 700, true, now);
  TEST_ASSERT_EQUAL(2, out.size());
  bool to_coordinator = false;
  bool to_other_member = false;
  for (const sls::Outgoing& o : out) {
    to_coordinator = to_coordinator || o.to == kMacs[0];
    to_other_member = to_other_member || o.to == kMacs[2];
    TEST_ASSERT_TRUE(sls::verify(o.message, kSecret));
    const sls::Presence* body = std::get_if<sls::Presence>(&o.message.body);
    TEST_ASSERT_NOT_NULL(body);
    TEST_ASSERT_TRUE(body->present);
    TEST_ASSERT_TRUE(body->open);
    TEST_ASSERT_EQUAL_UINT32(700, body->total_presence_ms);
  }
  TEST_ASSERT_TRUE(to_coordinator && to_other_member);
}

void test_a_member_that_hears_its_coordinator_sends_its_presence_to_the_coordinator_only() {
  sls::GroupSync coordinator = coordinator_unit();
  sls::GroupSync member = member_unit(kMacs[1]);
  TEST_ASSERT_TRUE(member.on_message(heartbeat_from(coordinator, 5000), kMacs[0], 5000));
  std::vector<sls::Outgoing> out = member.report_presence(true, 700, true, 6000);
  TEST_ASSERT_EQUAL(1, out.size());
  TEST_ASSERT_TRUE(out[0].to == kMacs[0]);
  TEST_ASSERT_TRUE(std::get<sls::Presence>(out[0].message.body).open);
}

// A Member (kMacs[1]) whose Coordinator named kMacs[2] as Motion master and then fell silent.
static sls::GroupSync silent_member_with_master(std::optional<sls::Mac> master = kMacs[2]) {
  sls::GroupSync coordinator = coordinator_unit();
  sls::GroupSync member = member_unit(kMacs[1]);
  member.on_message(heartbeat_from(coordinator, 1000, master), kMacs[0], 1000);
  return member;
}
static const uint32_t kSilentNow = 1000 + sls::kCoordinatorSilenceMs + 1;

// The Presence the Sensor unit at kMacs[2] sends to the Member at kMacs[1] while it is silent too.
static sls::Message presence_from_master(uint32_t total_ms = 10) {
  sls::GroupSync sensor = member_unit(kMacs[2]);
  for (const sls::Outgoing& o : sensor.report_presence(true, total_ms, true, kSilentNow))
    if (o.to == kMacs[1]) return o.message;
  return sls::Message{};
}

void test_a_silent_member_accepts_a_presence_from_the_motion_master() {
  sls::GroupSync member = silent_member_with_master();
  TEST_ASSERT_TRUE(member.silent(kSilentNow));
  TEST_ASSERT_TRUE(member.on_member_report(presence_from_master(), kMacs[2], kSilentNow));
}

void test_a_member_that_still_hears_its_coordinator_ignores_a_presence() {
  sls::GroupSync member = silent_member_with_master();
  TEST_ASSERT_FALSE(member.on_member_report(presence_from_master(), kMacs[2], 1000 + sls::kCoordinatorSilenceMs));
}

void test_a_silent_member_ignores_a_presence_from_a_unit_other_than_the_motion_master() {
  sls::GroupSync member = silent_member_with_master(kMacs[0]);
  TEST_ASSERT_FALSE(member.on_member_report(presence_from_master(), kMacs[2], kSilentNow));
}

void test_a_silent_member_ignores_a_presence_when_no_motion_master_is_known() {
  sls::GroupSync member = silent_member_with_master(std::nullopt);
  TEST_ASSERT_FALSE(member.on_member_report(presence_from_master(), kMacs[2], kSilentNow));
}

void test_a_silent_member_ignores_a_presence_whose_source_is_not_the_master() {
  sls::GroupSync member = silent_member_with_master();
  TEST_ASSERT_FALSE(member.on_member_report(presence_from_master(), kMacs[0], kSilentNow));
}

void test_a_silent_member_ignores_a_presence_with_a_bad_tag() {
  sls::GroupSync member = silent_member_with_master();
  sls::Message tampered = presence_from_master();
  std::get<sls::Presence>(tampered.body).present = false;
  TEST_ASSERT_FALSE(member.on_member_report(tampered, kMacs[2], kSilentNow));
}

void test_a_silent_member_ignores_a_replayed_or_older_presence() {
  sls::GroupSync member = silent_member_with_master();
  sls::Message older = presence_from_master(10);
  sls::Message newer = presence_from_master(20);
  TEST_ASSERT_TRUE(member.on_member_report(newer, kMacs[2], kSilentNow));
  TEST_ASSERT_FALSE(member.on_member_report(newer, kMacs[2], kSilentNow));
  TEST_ASSERT_FALSE(member.on_member_report(older, kMacs[2], kSilentNow));
}

void test_a_member_always_ignores_an_override_claim() {
  sls::GroupSync member = silent_member_with_master();
  sls::Message claim{sls::MessageType::OverrideClaim, kGroupId, kMacs[2], std::monostate{}};
  claim.auth.boot_count = kBootCount;
  claim.auth.counter = 77;
  sls::sign(claim, kSecret);
  TEST_ASSERT_FALSE(member.on_member_report(claim, kMacs[2], kSilentNow));
}

void test_a_scan_listens_on_home_outside_a_burst() {
  TEST_ASSERT_EQUAL(6, sls::scan_channel(6, sls::kScanBurstMs));
  TEST_ASSERT_EQUAL(6, sls::scan_channel(6, sls::kScanIntervalMs - 1));
}

void test_the_first_burst_listens_on_channel_one_when_home_is_elsewhere() {
  TEST_ASSERT_EQUAL(1, sls::scan_channel(6, 0));
  TEST_ASSERT_EQUAL(1, sls::scan_channel(6, sls::kScanBurstMs - 1));
}

void test_the_second_burst_listens_on_channel_two() {
  TEST_ASSERT_EQUAL(2, sls::scan_channel(6, sls::kScanIntervalMs));
}

void test_a_burst_skips_the_home_channel() {
  TEST_ASSERT_EQUAL(5, sls::scan_channel(6, 4 * sls::kScanIntervalMs));
  TEST_ASSERT_EQUAL(7, sls::scan_channel(6, 5 * sls::kScanIntervalMs));
}

void test_the_bursts_wrap_after_twelve_channels() {
  TEST_ASSERT_EQUAL(13, sls::scan_channel(6, 11 * sls::kScanIntervalMs));
  TEST_ASSERT_EQUAL(1, sls::scan_channel(6, 12 * sls::kScanIntervalMs));
}

void test_a_home_of_one_makes_the_first_burst_listen_on_two() {
  TEST_ASSERT_EQUAL(2, sls::scan_channel(1, 0));
}

static sls::Message heartbeat_on_channel(uint8_t channel, uint32_t now_ms) {
  sls::GroupSync coordinator = coordinator_unit();
  for (const sls::Outgoing& o : coordinator.on_tick(now_ms, channel))
    if (o.to == kMacs[1]) return o.message;
  return sls::Message{};
}

static sls::GroupSync member_on_channel(uint8_t channel) {
  sls::SyncRecord record{sls::SyncRole::Member, kMacs[0], 0};
  record.channel = channel;
  return sls::GroupSync(kMacs[1], kGroupId, table(), kSecret, record, kBootCount, sls::default_group_state());
}

void test_a_member_restores_its_home_channel_from_the_saved_record() {
  sls::GroupSync member = member_on_channel(9);
  TEST_ASSERT_EQUAL(9, member.record().channel);
}

void test_an_accepted_heartbeat_sets_the_home_channel() {
  sls::GroupSync member = member_on_channel(0);
  TEST_ASSERT_TRUE(member.on_message(heartbeat_on_channel(11, 1000), kMacs[0], 1000));
  TEST_ASSERT_EQUAL(11, member.record().channel);
}

void test_a_heartbeat_with_an_out_of_range_channel_leaves_the_home_channel() {
  sls::GroupSync member = member_on_channel(9);
  TEST_ASSERT_TRUE(member.on_message(heartbeat_on_channel(14, 1000), kMacs[0], 1000));
  TEST_ASSERT_EQUAL(9, member.record().channel);
}

void test_regrouping_keeps_the_home_channel() {
  sls::GroupSync member = member_on_channel(9);
  member.regroup(kGroupId + 1, table(), kSecret);
  TEST_ASSERT_EQUAL(9, member.record().channel);
}

void test_a_member_starts_on_its_restored_home_channel() {
  sls::GroupSync member = member_on_channel(9);
  TEST_ASSERT_EQUAL(9, member.listen_channel(0));
}

void test_a_member_with_no_known_channel_starts_on_the_pairing_channel() {
  sls::GroupSync member = member_on_channel(0);
  TEST_ASSERT_EQUAL(ESPNOW_PAIRING_CHANNEL, member.listen_channel(0));
}

void test_a_member_listens_on_its_home_channel_while_the_coordinator_is_heard() {
  sls::GroupSync member = member_on_channel(9);
  TEST_ASSERT_TRUE(member.on_message(heartbeat_on_channel(9, 5000), kMacs[0], 5000));
  TEST_ASSERT_EQUAL(9, member.listen_channel(5000 + sls::kCoordinatorSilenceMs));
}

void test_a_silent_member_scans_once_the_coordinator_was_quiet_for_more_than_ten_seconds() {
  sls::GroupSync member = member_on_channel(9);
  TEST_ASSERT_TRUE(member.on_message(heartbeat_on_channel(9, 5000), kMacs[0], 5000));
  const uint32_t silent_at = 5000 + sls::kCoordinatorSilenceMs;
  TEST_ASSERT_EQUAL(1, member.listen_channel(silent_at + 1));
  TEST_ASSERT_EQUAL(9, member.listen_channel(silent_at + sls::kScanBurstMs + 1));
  TEST_ASSERT_EQUAL(2, member.listen_channel(silent_at + sls::kScanIntervalMs + 1));
}

void test_an_undecided_unit_in_a_group_scans_like_a_member() {
  sls::GroupSync undecided(kMacs[2], kGroupId, table(), kSecret, sls::SyncRecord{sls::SyncRole::Undecided}, kBootCount,
                           sls::default_group_state());
  TEST_ASSERT_EQUAL(ESPNOW_PAIRING_CHANNEL, undecided.listen_channel(sls::kCoordinatorSilenceMs));
  TEST_ASSERT_EQUAL(2, undecided.listen_channel(sls::kCoordinatorSilenceMs + 1));
}

void test_a_coordinator_a_lone_unit_and_a_commissioning_unit_never_move_the_radio() {
  sls::GroupSync coordinator = coordinator_unit();
  sls::GroupSync alone(kMacs[1], kGroupId, std::vector<sls::Mac>{kMacs[1]}, kSecret,
                       sls::SyncRecord{sls::SyncRole::Member, kMacs[0], 0}, kBootCount, sls::default_group_state());
  sls::GroupSync commissioning = member_on_channel(9);
  commissioning.on_commissioning(true);
  TEST_ASSERT_EQUAL(0, coordinator.listen_channel(100000));
  TEST_ASSERT_EQUAL(0, alone.listen_channel(100000));
  TEST_ASSERT_EQUAL(0, commissioning.listen_channel(100000));
}

static const uint32_t kSilentAt = sls::kCoordinatorSilenceMs + 1000;

void test_a_silent_member_that_finds_the_coordinator_on_a_new_channel_announces_the_move_on_the_old_one() {
  sls::GroupSync member = member_on_channel(9);
  TEST_ASSERT_TRUE(member.on_message(heartbeat_on_channel(4, kSilentAt), kMacs[0], kSilentAt));
  TEST_ASSERT_EQUAL(4, member.record().channel);
  std::optional<sls::ChannelAnnouncement> announcement = member.channel_announcement();
  TEST_ASSERT_TRUE(announcement.has_value());
  TEST_ASSERT_EQUAL(9, announcement->old_channel);
  TEST_ASSERT_EQUAL(2, announcement->messages.size());
  for (const sls::Outgoing& o : announcement->messages) {
    TEST_ASSERT_TRUE(o.to != kMacs[1]);
    TEST_ASSERT_EQUAL(static_cast<int>(sls::MessageType::ChannelMoved), static_cast<int>(o.message.type));
    TEST_ASSERT_TRUE(sls::verify(o.message, kSecret));
    const sls::ChannelMoved* moved = std::get_if<sls::ChannelMoved>(&o.message.body);
    TEST_ASSERT_NOT_NULL(moved);
    TEST_ASSERT_EQUAL(4, moved->channel);
  }
}

void test_a_channel_announcement_is_handed_out_once() {
  sls::GroupSync member = member_on_channel(9);
  member.on_message(heartbeat_on_channel(4, kSilentAt), kMacs[0], kSilentAt);
  TEST_ASSERT_TRUE(member.channel_announcement().has_value());
  TEST_ASSERT_FALSE(member.channel_announcement().has_value());
}

void test_a_member_that_was_not_silent_follows_a_new_channel_without_announcing() {
  sls::GroupSync member = member_on_channel(9);
  TEST_ASSERT_TRUE(member.on_message(heartbeat_on_channel(4, 1000), kMacs[0], 1000));
  TEST_ASSERT_EQUAL(4, member.record().channel);
  TEST_ASSERT_FALSE(member.channel_announcement().has_value());
}

static sls::Message channel_moved(const sls::Mac& sender, uint8_t channel, uint32_t counter, bool signed_ok = true) {
  sls::Message message{sls::MessageType::ChannelMoved, kGroupId, sender, sls::ChannelMoved{channel}};
  message.auth.boot_count = kBootCount;
  message.auth.counter = counter;
  sls::sign(message, kSecret);
  if (!signed_ok) message.auth.tag[0] ^= 0xFF;
  return message;
}

void test_a_member_follows_a_channel_moved_from_another_group_member() {
  sls::GroupSync member = member_on_channel(9);
  TEST_ASSERT_TRUE(member.on_member_report(channel_moved(kMacs[2], 4, 10), kMacs[2], 1000));
  TEST_ASSERT_EQUAL(4, member.listen_channel(1000));
}

void test_a_member_ignores_a_channel_moved_with_a_bad_tag() {
  sls::GroupSync member = member_on_channel(9);
  TEST_ASSERT_FALSE(member.on_member_report(channel_moved(kMacs[2], 4, 10, false), kMacs[2], 1000));
  TEST_ASSERT_EQUAL(9, member.listen_channel(1000));
}

void test_a_member_ignores_a_channel_moved_from_a_non_member() {
  sls::GroupSync member = member_on_channel(9);
  const sls::Mac stranger = {0x02, 0, 0, 0, 0, 0x99};
  TEST_ASSERT_FALSE(member.on_member_report(channel_moved(stranger, 4, 10), stranger, 1000));
  TEST_ASSERT_EQUAL(9, member.listen_channel(1000));
}

void test_a_member_ignores_a_channel_moved_to_an_out_of_range_channel() {
  sls::GroupSync member = member_on_channel(9);
  TEST_ASSERT_FALSE(member.on_member_report(channel_moved(kMacs[2], 14, 10), kMacs[2], 1000));
  TEST_ASSERT_FALSE(member.on_member_report(channel_moved(kMacs[2], 0, 11), kMacs[2], 1000));
  TEST_ASSERT_EQUAL(9, member.listen_channel(1000));
}

void test_a_member_ignores_a_replayed_channel_moved() {
  sls::GroupSync member = member_on_channel(9);
  TEST_ASSERT_TRUE(member.on_member_report(channel_moved(kMacs[2], 4, 10), kMacs[2], 1000));
  TEST_ASSERT_FALSE(member.on_member_report(channel_moved(kMacs[2], 5, 10), kMacs[2], 1100));
  TEST_ASSERT_EQUAL(4, member.listen_channel(1100));
}

void test_a_coordinator_ignores_a_channel_moved() {
  sls::GroupSync coordinator = coordinator_unit();
  TEST_ASSERT_FALSE(coordinator.on_member_report(channel_moved(kMacs[2], 4, 10), kMacs[2], 1000));
  TEST_ASSERT_EQUAL(0, coordinator.record().channel);
}

// A Member (kMacs[1]) that heard `coordinator` name `master` as Motion master, went silent and opened locally.
static sls::GroupSync silent_member_opened_locally(sls::GroupSync& coordinator, std::optional<sls::Mac> master) {
  sls::GroupSync member = member_unit(kMacs[1]);
  member.on_message(heartbeat_from(coordinator, 1000, master), kMacs[0], 1000);
  member.on_local_change(sls::apply(sls::default_group_state(), sls::Command::open()), kSilentNow);
  return member;
}

void test_a_returning_member_takes_the_look_but_keeps_its_own_open_state() {
  sls::GroupSync coordinator = coordinator_unit();
  sls::GroupSync member = silent_member_opened_locally(coordinator, kMacs[2]);
  coordinator.on_local_change(sls::apply(sls::default_group_state(), sls::Command::set_blink_speed(60)), 0);
  const sls::GroupState wanted = coordinator.state();
  TEST_ASSERT_FALSE(wanted.open);
  TEST_ASSERT_TRUE(member.on_message(heartbeat_from(coordinator, kSilentNow + 100), kMacs[0], kSilentNow + 100));
  TEST_ASSERT_TRUE(member.state().open);
  TEST_ASSERT_EQUAL(wanted.look.blink_speed_pct, member.state().look.blink_speed_pct);
}

void test_a_returning_member_keeps_its_open_state_through_later_heartbeats() {
  sls::GroupSync coordinator = coordinator_unit();
  sls::GroupSync member = silent_member_opened_locally(coordinator, kMacs[2]);
  TEST_ASSERT_TRUE(member.on_message(heartbeat_from(coordinator, kSilentNow + 100), kMacs[0], kSilentNow + 100));
  TEST_ASSERT_TRUE(member.on_message(heartbeat_from(coordinator, kSilentNow + 2100), kMacs[0], kSilentNow + 2100));
  TEST_ASSERT_TRUE(member.state().open);
}

void test_a_returning_member_follows_the_coordinators_next_state_change_in_full() {
  sls::GroupSync coordinator = coordinator_unit();
  sls::GroupSync member = silent_member_opened_locally(coordinator, kMacs[2]);
  TEST_ASSERT_TRUE(member.on_message(heartbeat_from(coordinator, kSilentNow + 100), kMacs[0], kSilentNow + 100));
  const sls::GroupState blinking = sls::apply(coordinator.state(), sls::Command::set_blink(true));
  for (const sls::Outgoing& o : coordinator.on_local_change(blinking, kSilentNow + 200))
    if (o.to == kMacs[1]) member.on_message(o.message, kMacs[0], kSilentNow + 200);
  TEST_ASSERT_TRUE(member.state().look.blink_on);
  TEST_ASSERT_FALSE(member.state().open);
}

void test_a_returning_member_with_no_known_master_applies_the_first_message_in_full() {
  sls::GroupSync coordinator = coordinator_unit();
  sls::GroupSync member = silent_member_opened_locally(coordinator, std::nullopt);
  TEST_ASSERT_TRUE(member.on_message(heartbeat_from(coordinator, kSilentNow + 100), kMacs[0], kSilentNow + 100));
  TEST_ASSERT_FALSE(member.state().open);
}

// A Coordinator that already sent its Heartbeat at 0, so the next one is due only 2 s later.
static sls::GroupSync coordinator_after_first_heartbeat() {
  sls::GroupSync coordinator = coordinator_unit();
  coordinator.on_tick(0, 6, kMacs[2]);
  return coordinator;
}

static const sls::StateSync* state_sync_to(const std::vector<sls::Outgoing>& out, const sls::Mac& to) {
  for (const sls::Outgoing& o : out)
    if (o.to == to) return std::get_if<sls::StateSync>(&o.message.body);
  return nullptr;
}

void test_a_coordinator_adopts_the_masters_open_state_on_its_first_report() {
  sls::GroupSync coordinator = coordinator_after_first_heartbeat();
  TEST_ASSERT_TRUE(coordinator.on_master_presence(kMacs[2], true, kMacs[2], 500));
  TEST_ASSERT_TRUE(coordinator.state().open);
}

void test_a_coordinator_that_adopted_the_masters_state_sends_a_heartbeat_right_away() {
  sls::GroupSync coordinator = coordinator_after_first_heartbeat();
  TEST_ASSERT_TRUE(coordinator.on_master_presence(kMacs[2], true, kMacs[2], 500));
  const sls::StateSync* sync = state_sync_to(coordinator.on_tick(600, 6, kMacs[2]), kMacs[1]);
  TEST_ASSERT_NOT_NULL(sync);
  TEST_ASSERT_TRUE(sync->state.open);
  TEST_ASSERT_EQUAL_UINT32(1, sync->seq);
  TEST_ASSERT_EQUAL(0, coordinator.on_tick(700, 6, kMacs[2]).size());
}

void test_a_coordinator_ignores_a_report_within_ten_seconds_of_the_previous_one_from_the_master() {
  sls::GroupSync coordinator = coordinator_after_first_heartbeat();
  TEST_ASSERT_TRUE(coordinator.on_master_presence(kMacs[2], true, kMacs[2], 500));
  coordinator.on_tick(600, 6, kMacs[2]);
  coordinator.on_local_change(sls::apply(coordinator.state(), sls::Command::close()), 700);
  TEST_ASSERT_FALSE(coordinator.on_master_presence(kMacs[2], true, kMacs[2], 500 + sls::kCoordinatorSilenceMs));
  TEST_ASSERT_FALSE(coordinator.state().open);
}

void test_a_coordinator_adopts_again_when_the_master_was_quiet_for_more_than_ten_seconds() {
  sls::GroupSync coordinator = coordinator_after_first_heartbeat();
  TEST_ASSERT_TRUE(coordinator.on_master_presence(kMacs[2], true, kMacs[2], 500));
  coordinator.on_local_change(sls::apply(coordinator.state(), sls::Command::close()), 700);
  TEST_ASSERT_TRUE(coordinator.on_master_presence(kMacs[2], true, kMacs[2], 500 + sls::kCoordinatorSilenceMs + 1));
  TEST_ASSERT_TRUE(coordinator.state().open);
}

void test_a_coordinator_ignores_a_report_that_is_not_from_the_master() {
  sls::GroupSync coordinator = coordinator_after_first_heartbeat();
  TEST_ASSERT_FALSE(coordinator.on_master_presence(kMacs[1], true, kMacs[2], 500));
  TEST_ASSERT_FALSE(coordinator.on_master_presence(kMacs[2], true, std::nullopt, 500));
  TEST_ASSERT_FALSE(coordinator.state().open);
  TEST_ASSERT_EQUAL(0, coordinator.on_tick(600, 6, kMacs[2]).size());
}

void test_a_member_does_not_adopt_a_masters_state() {
  sls::GroupSync member = member_unit(kMacs[1]);
  TEST_ASSERT_FALSE(member.on_master_presence(kMacs[2], true, kMacs[2], 500));
  TEST_ASSERT_FALSE(member.state().open);
}

void test_a_local_change_to_the_adopted_state_sends_nothing() {
  sls::GroupSync coordinator = coordinator_after_first_heartbeat();
  TEST_ASSERT_TRUE(coordinator.on_master_presence(kMacs[2], true, kMacs[2], 500));
  TEST_ASSERT_EQUAL(0, coordinator.on_local_change(sls::apply(sls::default_group_state(), sls::Command::open()), 600).size());
}

void test_a_returning_member_holding_open_applies_the_coordinators_adopted_open_state_in_full() {
  sls::GroupSync coordinator = coordinator_unit();
  sls::GroupSync member = silent_member_opened_locally(coordinator, kMacs[2]);
  TEST_ASSERT_TRUE(member.on_message(heartbeat_from(coordinator, kSilentNow + 100), kMacs[0], kSilentNow + 100));
  TEST_ASSERT_TRUE(coordinator.on_master_presence(kMacs[2], false, kMacs[2], kSilentNow + 200));
  TEST_ASSERT_TRUE(member.on_message(heartbeat_from(coordinator, kSilentNow + 300), kMacs[0], kSilentNow + 300));
  TEST_ASSERT_FALSE(member.state().open);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_a_member_follows_a_channel_moved_from_another_group_member);
  RUN_TEST(test_a_member_ignores_a_channel_moved_with_a_bad_tag);
  RUN_TEST(test_a_member_ignores_a_channel_moved_from_a_non_member);
  RUN_TEST(test_a_member_ignores_a_channel_moved_to_an_out_of_range_channel);
  RUN_TEST(test_a_member_ignores_a_replayed_channel_moved);
  RUN_TEST(test_a_coordinator_ignores_a_channel_moved);
  RUN_TEST(test_a_silent_member_that_finds_the_coordinator_on_a_new_channel_announces_the_move_on_the_old_one);
  RUN_TEST(test_a_channel_announcement_is_handed_out_once);
  RUN_TEST(test_a_member_that_was_not_silent_follows_a_new_channel_without_announcing);
  RUN_TEST(test_a_member_starts_on_its_restored_home_channel);
  RUN_TEST(test_a_member_with_no_known_channel_starts_on_the_pairing_channel);
  RUN_TEST(test_a_member_listens_on_its_home_channel_while_the_coordinator_is_heard);
  RUN_TEST(test_a_silent_member_scans_once_the_coordinator_was_quiet_for_more_than_ten_seconds);
  RUN_TEST(test_an_undecided_unit_in_a_group_scans_like_a_member);
  RUN_TEST(test_a_coordinator_a_lone_unit_and_a_commissioning_unit_never_move_the_radio);
  RUN_TEST(test_a_member_restores_its_home_channel_from_the_saved_record);
  RUN_TEST(test_an_accepted_heartbeat_sets_the_home_channel);
  RUN_TEST(test_a_heartbeat_with_an_out_of_range_channel_leaves_the_home_channel);
  RUN_TEST(test_regrouping_keeps_the_home_channel);
  RUN_TEST(test_a_scan_listens_on_home_outside_a_burst);
  RUN_TEST(test_the_first_burst_listens_on_channel_one_when_home_is_elsewhere);
  RUN_TEST(test_the_second_burst_listens_on_channel_two);
  RUN_TEST(test_a_burst_skips_the_home_channel);
  RUN_TEST(test_the_bursts_wrap_after_twelve_channels);
  RUN_TEST(test_a_home_of_one_makes_the_first_burst_listen_on_two);
  RUN_TEST(test_a_member_is_silent_once_the_coordinator_was_quiet_for_more_than_ten_seconds);
  RUN_TEST(test_a_member_that_never_heard_the_coordinator_counts_silence_from_start);
  RUN_TEST(test_a_coordinator_or_undecided_unit_is_never_silent);
  RUN_TEST(test_a_member_reports_the_motion_master_named_in_the_coordinators_heartbeat);
  RUN_TEST(test_a_member_reports_no_motion_master_when_the_heartbeat_names_none);
  RUN_TEST(test_a_silent_member_keeps_a_local_change_and_sends_nothing);
  RUN_TEST(test_a_member_that_hears_its_coordinator_still_ignores_a_local_change);
  RUN_TEST(test_a_silent_member_sends_its_presence_to_every_other_group_member);
  RUN_TEST(test_a_member_that_hears_its_coordinator_sends_its_presence_to_the_coordinator_only);
  RUN_TEST(test_a_silent_member_accepts_a_presence_from_the_motion_master);
  RUN_TEST(test_a_member_that_still_hears_its_coordinator_ignores_a_presence);
  RUN_TEST(test_a_silent_member_ignores_a_presence_from_a_unit_other_than_the_motion_master);
  RUN_TEST(test_a_silent_member_ignores_a_presence_when_no_motion_master_is_known);
  RUN_TEST(test_a_silent_member_ignores_a_presence_whose_source_is_not_the_master);
  RUN_TEST(test_a_silent_member_ignores_a_presence_with_a_bad_tag);
  RUN_TEST(test_a_silent_member_ignores_a_replayed_or_older_presence);
  RUN_TEST(test_a_member_always_ignores_an_override_claim);
  RUN_TEST(test_a_returning_member_takes_the_look_but_keeps_its_own_open_state);
  RUN_TEST(test_a_returning_member_keeps_its_open_state_through_later_heartbeats);
  RUN_TEST(test_a_returning_member_follows_the_coordinators_next_state_change_in_full);
  RUN_TEST(test_a_returning_member_with_no_known_master_applies_the_first_message_in_full);
  RUN_TEST(test_a_coordinator_adopts_the_masters_open_state_on_its_first_report);
  RUN_TEST(test_a_coordinator_that_adopted_the_masters_state_sends_a_heartbeat_right_away);
  RUN_TEST(test_a_coordinator_ignores_a_report_within_ten_seconds_of_the_previous_one_from_the_master);
  RUN_TEST(test_a_coordinator_adopts_again_when_the_master_was_quiet_for_more_than_ten_seconds);
  RUN_TEST(test_a_coordinator_ignores_a_report_that_is_not_from_the_master);
  RUN_TEST(test_a_member_does_not_adopt_a_masters_state);
  RUN_TEST(test_a_local_change_to_the_adopted_state_sends_nothing);
  RUN_TEST(test_a_returning_member_holding_open_applies_the_coordinators_adopted_open_state_in_full);
  return UNITY_END();
}
