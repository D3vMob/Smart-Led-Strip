#include <unity.h>
#include <vector>
#include "group_sync.h"

void setUp() {}
void tearDown() {}

static const sls::Mac kMacs[3] = {{0x02, 0, 0, 0, 0, 0x01}, {0x02, 0, 0, 0, 0, 0x02}, {0x02, 0, 0, 0, 0, 0x03}};
static const uint32_t kGroupId = 0xABCD;
static const uint32_t kBootNonce = 0x1234;
static const sls::GroupSecret kSecret = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                                         17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
static const sls::GroupSecret kOtherSecret = {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
                                              9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9};

// A message signed with the Group secret, as a genuine Member would send it.
// A message with no auth stamp yet gets this boot's nonce and the next counter, like a live sender.
static sls::Message signed_msg(sls::Message message, const sls::GroupSecret& secret = kSecret) {
  static uint32_t next_counter = 0;
  if (message.auth.counter == 0) {
    message.auth.boot_nonce = kBootNonce;
    message.auth.counter = ++next_counter;
  }
  sls::sign(message, secret);
  return message;
}

static void assert_same_state(const sls::GroupState& expected, const sls::GroupState& actual) {
  TEST_ASSERT_EQUAL(expected.open, actual.open);
  TEST_ASSERT_EQUAL(expected.motion_enabled, actual.motion_enabled);
  TEST_ASSERT_EQUAL(static_cast<int>(expected.look.color_mode), static_cast<int>(actual.look.color_mode));
  TEST_ASSERT_EQUAL(expected.look.hue_deg, actual.look.hue_deg);
  TEST_ASSERT_EQUAL(expected.look.saturation, actual.look.saturation);
  TEST_ASSERT_EQUAL(expected.look.color_temp_mireds, actual.look.color_temp_mireds);
  TEST_ASSERT_EQUAL(expected.look.brightness_pct, actual.look.brightness_pct);
  TEST_ASSERT_EQUAL(expected.look.blink_on, actual.look.blink_on);
  TEST_ASSERT_EQUAL(expected.look.blink_speed_pct, actual.look.blink_speed_pct);
}

// Delivers each Outgoing to the Unit whose MAC it is addressed to.
static void deliver(std::vector<sls::GroupSync>& units, const std::vector<sls::Outgoing>& outgoing, uint32_t now_ms) {
  for (const sls::Outgoing& o : outgoing)
    for (size_t i = 0; i < units.size(); ++i)
      if (kMacs[i] == o.to) units[i].on_message(o.message, o.message.sender, now_ms);
}

void test_a_change_on_the_coordinator_reaches_every_member() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupState initial = sls::default_group_state();
  std::vector<sls::GroupSync> units;
  units.emplace_back(kMacs[0], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Coordinator}, kBootNonce, initial);
  units.emplace_back(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member}, kBootNonce, initial);
  units.emplace_back(kMacs[2], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member}, kBootNonce, initial);

  sls::GroupState changed = sls::apply(initial, sls::Command::open());
  changed = sls::apply(changed, sls::Command::set_hue_sat(200, 80));

  std::vector<sls::Outgoing> out = units[0].on_local_change(changed, 1000);
  deliver(units, out, 1000);

  TEST_ASSERT_EQUAL(2, out.size());
  for (const sls::Outgoing& o : out) TEST_ASSERT_FALSE(o.to == kMacs[0]);
  assert_same_state(changed, units[0].state());
  assert_same_state(changed, units[1].state());
  assert_same_state(changed, units[2].state());
}

void test_a_member_drops_a_state_with_an_older_sequence() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupState initial = sls::default_group_state();
  sls::GroupSync coordinator(kMacs[0], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Coordinator}, kBootNonce, initial);
  sls::GroupSync member(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member}, kBootNonce, initial);

  sls::GroupState a = sls::apply(initial, sls::Command::open());
  sls::GroupState b = sls::apply(a, sls::Command::set_hue_sat(200, 80));
  std::vector<sls::Outgoing> msgs_a = coordinator.on_local_change(a, 1000);
  std::vector<sls::Outgoing> msgs_b = coordinator.on_local_change(b, 1100);

  for (const sls::Outgoing& o : msgs_b) if (o.to == kMacs[1]) member.on_message(o.message, o.message.sender, 1200);
  for (const sls::Outgoing& o : msgs_a) if (o.to == kMacs[1]) member.on_message(o.message, o.message.sender, 1300);

  assert_same_state(b, member.state());
}

static sls::Message state_message(const sls::GroupState& state, uint32_t seq) {
  return signed_msg(sls::Message{sls::MessageType::State, kGroupId, kMacs[0], sls::StateSync{state, seq, 0, 0}});
}

void test_a_member_accepts_a_newer_sequence_across_32_bit_wrap_around() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupState initial = sls::default_group_state();
  sls::GroupSync member(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member}, kBootNonce, initial);

  sls::GroupState a = sls::apply(initial, sls::Command::open());
  sls::GroupState b = sls::apply(a, sls::Command::set_hue_sat(200, 80));
  sls::GroupState c = sls::apply(a, sls::Command::set_hue_sat(10, 30));

  member.on_message(state_message(a, 0xFFFFFFFFu), kMacs[0], 1000);
  member.on_message(state_message(b, 1), kMacs[0], 1100);
  member.on_message(state_message(c, 0xFFFFFFFEu), kMacs[0], 1200);

  assert_same_state(b, member.state());
}

void test_the_coordinator_sends_a_heartbeat_to_every_other_member_every_two_seconds() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupSync coordinator(kMacs[0], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Coordinator}, kBootNonce, sls::default_group_state());
  sls::GroupState changed = sls::apply(sls::default_group_state(), sls::Command::open());
  changed = sls::apply(changed, sls::Command::set_blink(true));
  uint32_t last_seq = 0;
  for (const sls::Outgoing& o : coordinator.on_local_change(changed, 0))
    last_seq = std::get<sls::StateSync>(o.message.body).seq;

  const uint32_t times[] = {0, 500, 1999, 2000, 3999, 4000};
  const bool expect_heartbeat[] = {true, false, false, true, false, true};
  for (size_t i = 0; i < 6; ++i) {
    std::vector<sls::Outgoing> out = coordinator.on_tick(times[i], 6);
    if (!expect_heartbeat[i]) {
      TEST_ASSERT_EQUAL(0, out.size());
      continue;
    }
    TEST_ASSERT_EQUAL(2, out.size());
    for (const sls::Outgoing& o : out) {
      TEST_ASSERT_FALSE(o.to == kMacs[0]);
      TEST_ASSERT_EQUAL(static_cast<int>(sls::MessageType::Heartbeat), static_cast<int>(o.message.type));
      const sls::StateSync& sync = std::get<sls::StateSync>(o.message.body);
      TEST_ASSERT_EQUAL(6, sync.wifi_channel);
      TEST_ASSERT_EQUAL(last_seq, sync.seq);
      TEST_ASSERT_EQUAL(coordinator.blink_phase_ms(times[i]), sync.blink_phase_ms);
      assert_same_state(coordinator.state(), sync.state);
    }
  }
}

void test_a_member_or_undecided_unit_sends_no_heartbeat() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupSync member(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member}, kBootNonce, sls::default_group_state());
  sls::GroupSync undecided(kMacs[2], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Undecided}, kBootNonce, sls::default_group_state());
  TEST_ASSERT_EQUAL(0, member.on_tick(0, 6).size());
  TEST_ASSERT_EQUAL(0, undecided.on_tick(0, 6).size());
}

void test_a_member_hearing_a_heartbeat_records_when() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupSync member(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member}, kBootNonce, sls::default_group_state());
  sls::Message heartbeat = signed_msg(sls::Message{sls::MessageType::Heartbeat, kGroupId, kMacs[0],
                                                   sls::StateSync{sls::default_group_state(), 5, 0, 6}});
  member.on_message(heartbeat, kMacs[0], 2345);
  TEST_ASSERT_EQUAL(2345, member.last_heard_ms());
}

static sls::Message heartbeat_message(const sls::GroupState& state, uint32_t seq) {
  return signed_msg(sls::Message{sls::MessageType::Heartbeat, kGroupId, kMacs[0], sls::StateSync{state, seq, 0, 6}});
}

void test_a_member_that_missed_a_state_catches_up_on_the_next_heartbeat() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupState initial = sls::default_group_state();
  std::vector<sls::GroupSync> units;
  units.emplace_back(kMacs[0], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Coordinator}, kBootNonce, initial);
  units.emplace_back(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member}, kBootNonce, initial);
  units.emplace_back(kMacs[2], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member}, kBootNonce, initial);
  sls::GroupState changed = sls::apply(initial, sls::Command::open());
  changed = sls::apply(changed, sls::Command::set_hue_sat(200, 80));

  for (const sls::Outgoing& o : units[0].on_local_change(changed, 1000))
    if (o.to == kMacs[2]) units[2].on_message(o.message, o.message.sender, 1000);
  TEST_ASSERT_FALSE(units[1].state().open);

  deliver(units, units[0].on_tick(2000, 6), 2000);

  assert_same_state(changed, units[1].state());
  assert_same_state(changed, units[2].state());
}

void test_a_member_follows_a_rebooted_coordinator_after_its_next_heartbeat() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupState initial = sls::default_group_state();
  sls::GroupSync member(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member}, kBootNonce, initial);
  sls::GroupState old_state = sls::apply(initial, sls::Command::open());
  member.on_message(state_message(old_state, 57), kMacs[0], 1000);

  sls::GroupSync rebooted(kMacs[0], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Coordinator}, kBootNonce + 1, initial);
  sls::GroupState first = sls::apply(initial, sls::Command::set_hue_sat(200, 80));
  for (const sls::Outgoing& o : rebooted.on_local_change(first, 5000))
    if (o.to == kMacs[1]) member.on_message(o.message, o.message.sender, 5000);
  assert_same_state(old_state, member.state());

  for (const sls::Outgoing& o : rebooted.on_tick(6000, 6))
    if (o.to == kMacs[1]) member.on_message(o.message, o.message.sender, 6000);
  assert_same_state(first, member.state());

  sls::GroupState second = sls::apply(first, sls::Command::set_hue_sat(10, 30));
  for (const sls::Outgoing& o : rebooted.on_local_change(second, 7000))
    if (o.to == kMacs[1]) member.on_message(o.message, o.message.sender, 7000);
  assert_same_state(second, member.state());
}

void test_a_rebooted_member_catches_up_on_the_next_heartbeat() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupState initial = sls::default_group_state();
  sls::GroupSync coordinator(kMacs[0], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Coordinator}, kBootNonce, initial);
  sls::GroupState changed = sls::apply(initial, sls::Command::open());
  coordinator.on_local_change(changed, 1000);

  sls::GroupSync rebooted(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member}, kBootNonce, initial);
  for (const sls::Outgoing& o : coordinator.on_tick(2000, 6))
    if (o.to == kMacs[1]) rebooted.on_message(o.message, o.message.sender, 2000);

  assert_same_state(changed, rebooted.state());
}

static sls::GroupState blinking_state() {
  sls::GroupState state = sls::apply(sls::default_group_state(), sls::Command::open());
  state = sls::apply(state, sls::Command::set_blink(true));
  return sls::apply(state, sls::Command::set_blink_speed(20));  // 1000 ms period
}

static sls::Message heartbeat_with_phase(uint32_t seq, uint32_t phase_ms) {
  return signed_msg(sls::Message{sls::MessageType::Heartbeat, kGroupId, kMacs[0], sls::StateSync{blinking_state(), seq, phase_ms, 6}});
}

static sls::GroupSync blinking_member() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupSync member(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member}, kBootNonce, sls::default_group_state());
  member.on_message(state_message(blinking_state(), 1), kMacs[0], 0);
  return member;
}

void test_a_member_realigns_its_blink_clock_when_drift_exceeds_the_threshold() {
  sls::GroupSync member = blinking_member();
  member.on_message(heartbeat_with_phase(1, 25), kMacs[0], 5000);
  TEST_ASSERT_EQUAL(25, member.blink_phase_ms(5000));
  TEST_ASSERT_EQUAL(125, member.blink_phase_ms(5100));
}

void test_a_member_leaves_its_blink_clock_alone_when_drift_is_within_the_threshold() {
  sls::GroupSync member = blinking_member();
  member.on_message(heartbeat_with_phase(1, 15), kMacs[0], 5000);
  TEST_ASSERT_EQUAL(0, member.blink_phase_ms(5000));
}

void test_blink_drift_is_measured_around_the_period() {
  sls::GroupSync member = blinking_member();
  TEST_ASSERT_EQUAL(990, member.blink_phase_ms(4990));
  member.on_message(heartbeat_with_phase(1, 5), kMacs[0], 4990);
  TEST_ASSERT_EQUAL(990, member.blink_phase_ms(4990));
}

void test_a_member_blinks_in_step_with_the_coordinator_after_the_first_heartbeat() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupSync coordinator(kMacs[0], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Coordinator}, kBootNonce, sls::default_group_state());
  sls::GroupSync member(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member}, kBootNonce, sls::default_group_state());
  for (const sls::Outgoing& o : coordinator.on_local_change(blinking_state(), 0))
    if (o.to == kMacs[1]) member.on_message(o.message, o.message.sender, 0);
  sls::Message knock = heartbeat_with_phase(1, 300);
  knock.auth.boot_nonce = kBootNonce;
  knock.auth.counter = 2;  // between the coordinator's State (1) and its next Heartbeat (3)
  sls::sign(knock, kSecret);
  member.on_message(knock, kMacs[0], 1000);  // knocks the Member's clock off
  TEST_ASSERT_NOT_EQUAL(coordinator.blink_phase_ms(1000), member.blink_phase_ms(1000));

  for (const sls::Outgoing& o : coordinator.on_tick(2000, 6))
    if (o.to == kMacs[1]) member.on_message(o.message, o.message.sender, 2000);

  for (uint32_t t = 2000; t <= 3500; t += 250) {
    int32_t diff = static_cast<int32_t>(member.blink_phase_ms(t)) - static_cast<int32_t>(coordinator.blink_phase_ms(t));
    TEST_ASSERT_TRUE(diff >= -static_cast<int32_t>(sls::kBlinkRealignMs) && diff <= static_cast<int32_t>(sls::kBlinkRealignMs));
  }
}

static std::vector<sls::GroupSync> make_units(sls::SyncRole role) {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  std::vector<sls::GroupSync> units;
  for (size_t i = 0; i < 3; ++i) units.emplace_back(kMacs[i], kGroupId, table, kSecret, sls::SyncRecord{role}, kBootNonce, sls::default_group_state());
  return units;
}

void test_the_first_commissioned_unit_becomes_coordinator_and_silences_the_others() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Undecided);
  for (const sls::GroupSync& u : units) {
    TEST_ASSERT_EQUAL(static_cast<int>(sls::SyncRole::Undecided), static_cast<int>(u.role()));
    TEST_ASSERT_TRUE(u.matter_enabled());
  }

  std::vector<sls::Outgoing> out = units[0].on_commissioned(1000);

  TEST_ASSERT_EQUAL(static_cast<int>(sls::SyncRole::Coordinator), static_cast<int>(units[0].role()));
  TEST_ASSERT_TRUE(units[0].matter_enabled());
  TEST_ASSERT_EQUAL(2, out.size());
  for (const sls::Outgoing& o : out) {
    TEST_ASSERT_FALSE(o.to == kMacs[0]);
    TEST_ASSERT_EQUAL(static_cast<int>(sls::MessageType::MatterSilent), static_cast<int>(o.message.type));
    TEST_ASSERT_EQUAL(kGroupId, o.message.group_id);
    TEST_ASSERT_TRUE(o.message.sender == kMacs[0]);
    TEST_ASSERT_TRUE(std::holds_alternative<std::monostate>(o.message.body));
  }
  deliver(units, out, 1000);
  for (size_t i = 1; i < 3; ++i) {
    TEST_ASSERT_EQUAL(static_cast<int>(sls::SyncRole::Member), static_cast<int>(units[i].role()));
    TEST_ASSERT_FALSE(units[i].matter_enabled());
  }
}

void test_a_coordinator_stays_coordinator_when_it_hears_matter_silent() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Coordinator);
  units[0].on_message(signed_msg(sls::Message{sls::MessageType::MatterSilent, kGroupId, kMacs[1], std::monostate{}}), kMacs[1], 1000);
  TEST_ASSERT_EQUAL(static_cast<int>(sls::SyncRole::Coordinator), static_cast<int>(units[0].role()));
  TEST_ASSERT_TRUE(units[0].matter_enabled());
}

void test_a_persisted_member_is_matter_silent_from_boot() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Member);
  TEST_ASSERT_EQUAL(static_cast<int>(sls::SyncRole::Member), static_cast<int>(units[1].role()));
  TEST_ASSERT_FALSE(units[1].matter_enabled());
}

void test_a_persisted_coordinator_sends_heartbeats_without_being_commissioned_again() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Coordinator);
  TEST_ASSERT_EQUAL(static_cast<int>(sls::SyncRole::Coordinator), static_cast<int>(units[0].role()));
  TEST_ASSERT_EQUAL(2, units[0].on_tick(0, 6).size());
}

void test_a_member_never_relays_a_local_change() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Member);
  TEST_ASSERT_EQUAL(0, units[1].on_local_change(sls::apply(sls::default_group_state(), sls::Command::open()), 1000).size());
}

void test_a_unit_ignores_messages_from_another_group() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Undecided);
  const uint32_t other_group = kGroupId + 1;
  sls::GroupState changed = sls::apply(sls::default_group_state(), sls::Command::open());
  units[1].on_message(signed_msg(sls::Message{sls::MessageType::State, other_group, kMacs[0], sls::StateSync{changed, 7, 0, 6}}), kMacs[0], 1000);
  units[1].on_message(signed_msg(sls::Message{sls::MessageType::MatterSilent, other_group, kMacs[0], std::monostate{}}), kMacs[0], 1000);
  TEST_ASSERT_FALSE(units[1].state().open);
  TEST_ASSERT_EQUAL(static_cast<int>(sls::SyncRole::Undecided), static_cast<int>(units[1].role()));
  TEST_ASSERT_TRUE(units[1].matter_enabled());
}

void test_a_sync_message_from_a_sender_outside_the_group_is_ignored() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Undecided);
  const sls::Mac stranger = {0x02, 0, 0, 0, 0, 0x99};
  sls::GroupState changed = sls::apply(sls::default_group_state(), sls::Command::open());

  const bool silent = units[1].on_message(signed_msg(sls::Message{sls::MessageType::MatterSilent, kGroupId, stranger, std::monostate{}}), stranger, 1000);
  const bool state = units[1].on_message(signed_msg(sls::Message{sls::MessageType::State, kGroupId, stranger, sls::StateSync{changed, 7, 0, 6}}), stranger, 1100);
  const bool heartbeat = units[1].on_message(signed_msg(sls::Message{sls::MessageType::Heartbeat, kGroupId, stranger, sls::StateSync{changed, 8, 0, 6}}), stranger, 1200);

  TEST_ASSERT_FALSE(silent);
  TEST_ASSERT_FALSE(state);
  TEST_ASSERT_FALSE(heartbeat);
  TEST_ASSERT_FALSE(units[1].state().open);
  TEST_ASSERT_EQUAL(static_cast<int>(sls::SyncRole::Undecided), static_cast<int>(units[1].role()));
  TEST_ASSERT_TRUE(units[1].matter_enabled());
  TEST_ASSERT_EQUAL(0, units[1].last_heard_ms());
}

void test_a_sync_message_whose_source_differs_from_its_claimed_sender_is_ignored() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Undecided);
  sls::GroupState changed = sls::apply(sls::default_group_state(), sls::Command::open());

  const bool silent = units[1].on_message(signed_msg(sls::Message{sls::MessageType::MatterSilent, kGroupId, kMacs[0], std::monostate{}}), kMacs[2], 1000);
  const bool state = units[1].on_message(signed_msg(sls::Message{sls::MessageType::State, kGroupId, kMacs[0], sls::StateSync{changed, 7, 0, 6}}), kMacs[2], 1100);
  const bool heartbeat = units[1].on_message(signed_msg(sls::Message{sls::MessageType::Heartbeat, kGroupId, kMacs[0], sls::StateSync{changed, 8, 0, 6}}), kMacs[2], 1200);

  TEST_ASSERT_FALSE(silent);
  TEST_ASSERT_FALSE(state);
  TEST_ASSERT_FALSE(heartbeat);
  TEST_ASSERT_FALSE(units[1].state().open);
  TEST_ASSERT_EQUAL(static_cast<int>(sls::SyncRole::Undecided), static_cast<int>(units[1].role()));
  TEST_ASSERT_TRUE(units[1].matter_enabled());
  TEST_ASSERT_EQUAL(0, units[1].last_heard_ms());
}

void test_a_members_record_names_the_coordinator_it_learned_from_an_authenticated_matter_silent() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Undecided);
  units[1].on_message(signed_msg(sls::Message{sls::MessageType::MatterSilent, kGroupId, kMacs[0], std::monostate{}}), kMacs[0], 1000);

  const sls::SyncRecord record = units[1].record();
  TEST_ASSERT_EQUAL(static_cast<int>(sls::SyncRole::Member), static_cast<int>(record.role));
  TEST_ASSERT_TRUE(record.coordinator.has_value());
  TEST_ASSERT_TRUE(kMacs[0] == *record.coordinator);
}

void test_an_undecided_unit_that_only_got_a_forged_frame_has_no_coordinator_in_its_record() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Undecided);
  units[1].on_message(signed_msg(sls::Message{sls::MessageType::MatterSilent, kGroupId, kMacs[0], std::monostate{}}, kOtherSecret), kMacs[0], 1000);

  const sls::SyncRecord record = units[1].record();
  TEST_ASSERT_EQUAL(static_cast<int>(sls::SyncRole::Undecided), static_cast<int>(record.role));
  TEST_ASSERT_FALSE(record.coordinator.has_value());
}

void test_a_coordinators_record_has_no_coordinator_to_follow() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Coordinator);

  const sls::SyncRecord record = units[0].record();
  TEST_ASSERT_EQUAL(static_cast<int>(sls::SyncRole::Coordinator), static_cast<int>(record.role));
  TEST_ASSERT_FALSE(record.coordinator.has_value());
}

void test_the_coordinator_pin_survives_a_restart() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupSync member(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member, kMacs[0]}, kBootNonce,
                        sls::default_group_state());
  sls::GroupState changed = sls::apply(sls::default_group_state(), sls::Command::open());

  const bool from_other_member = member.on_message(
      signed_msg(sls::Message{sls::MessageType::Heartbeat, kGroupId, kMacs[2], sls::StateSync{changed, 7, 0, 6}}), kMacs[2], 1000);
  TEST_ASSERT_FALSE(from_other_member);
  TEST_ASSERT_FALSE(member.state().open);

  const bool from_coordinator = member.on_message(
      signed_msg(sls::Message{sls::MessageType::Heartbeat, kGroupId, kMacs[0], sls::StateSync{changed, 8, 0, 6}}), kMacs[0], 1100);
  TEST_ASSERT_TRUE(from_coordinator);
  TEST_ASSERT_TRUE(member.state().open);
}

void test_an_undecided_unit_applies_a_local_change_but_relays_nothing() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Undecided);
  sls::GroupState changed = sls::apply(sls::default_group_state(), sls::Command::open());
  TEST_ASSERT_EQUAL(0, units[0].on_local_change(changed, 1000).size());
  assert_same_state(changed, units[0].state());
}

void test_a_stale_state_still_counts_as_hearing_the_coordinator() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Member);
  sls::GroupState a = sls::apply(sls::default_group_state(), sls::Command::open());
  units[1].on_message(state_message(a, 5), kMacs[0], 1000);
  units[1].on_message(state_message(sls::default_group_state(), 4), kMacs[0], 2500);
  TEST_ASSERT_EQUAL(2500, units[1].last_heard_ms());
  assert_same_state(a, units[1].state());
}

void test_a_local_change_equal_to_the_current_state_is_a_no_op() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Coordinator);
  sls::GroupState a = sls::apply(sls::default_group_state(), sls::Command::open());
  sls::GroupState b = sls::apply(a, sls::Command::set_hue_sat(200, 80));

  std::vector<sls::Outgoing> first = units[0].on_local_change(a, 1000);
  TEST_ASSERT_EQUAL(2, first.size());
  const uint32_t n = std::get<sls::StateSync>(first[0].message.body).seq;

  TEST_ASSERT_EQUAL(0, units[0].on_local_change(a, 1100).size());

  std::vector<sls::Outgoing> next = units[0].on_local_change(b, 1200);
  TEST_ASSERT_EQUAL(2, next.size());
  TEST_ASSERT_EQUAL(n + 1, std::get<sls::StateSync>(next[0].message.body).seq);
}

void test_the_coordinator_ignores_states_and_heartbeats_it_receives() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Coordinator);
  sls::GroupState mine = blinking_state();
  units[0].on_local_change(mine, 1000);
  const uint32_t phase_before = units[0].blink_phase_ms(5000);

  sls::GroupState other = sls::apply(sls::default_group_state(), sls::Command::set_hue_sat(200, 80));
  units[0].on_message(signed_msg(sls::Message{sls::MessageType::State, kGroupId, kMacs[1], sls::StateSync{other, 50, 0, 0}}), kMacs[1], 4000);
  units[0].on_message(signed_msg(sls::Message{sls::MessageType::Heartbeat, kGroupId, kMacs[1], sls::StateSync{other, 60, 300, 6}}), kMacs[1], 5000);

  assert_same_state(mine, units[0].state());
  TEST_ASSERT_EQUAL(phase_before, units[0].blink_phase_ms(5000));
}

void test_an_undecided_unit_that_hears_a_state_becomes_a_member() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupState initial = sls::default_group_state();
  sls::GroupSync coordinator(kMacs[0], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Coordinator}, kBootNonce, initial);
  sls::GroupSync undecided(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Undecided}, kBootNonce, initial);
  sls::GroupState changed = sls::apply(initial, sls::Command::open());

  for (const sls::Outgoing& o : coordinator.on_local_change(changed, 1000))
    if (o.to == kMacs[1]) undecided.on_message(o.message, o.message.sender, 1000);

  TEST_ASSERT_EQUAL(static_cast<int>(sls::SyncRole::Member), static_cast<int>(undecided.role()));
  TEST_ASSERT_FALSE(undecided.matter_enabled());
  assert_same_state(changed, undecided.state());
}

void test_an_undecided_unit_that_hears_a_heartbeat_becomes_a_member() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupState initial = sls::default_group_state();
  sls::GroupSync coordinator(kMacs[0], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Coordinator}, kBootNonce, initial);
  sls::GroupSync undecided(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Undecided}, kBootNonce, initial);
  coordinator.on_local_change(sls::apply(initial, sls::Command::open()), 500);

  for (const sls::Outgoing& o : coordinator.on_tick(1000, 6))
    if (o.to == kMacs[1]) undecided.on_message(o.message, o.message.sender, 1000);

  TEST_ASSERT_EQUAL(static_cast<int>(sls::SyncRole::Member), static_cast<int>(undecided.role()));
  TEST_ASSERT_FALSE(undecided.matter_enabled());
  assert_same_state(coordinator.state(), undecided.state());
}

void test_a_member_that_hears_nothing_hunts_across_the_channels_after_six_seconds() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Member);
  const sls::GroupSync& member = units[1];
  TEST_ASSERT_EQUAL(0, member.listen_channel(5999));
  TEST_ASSERT_EQUAL(1, member.listen_channel(6000));
  TEST_ASSERT_EQUAL(1, member.listen_channel(8499));
  TEST_ASSERT_EQUAL(2, member.listen_channel(8500));
  TEST_ASSERT_EQUAL(13, member.listen_channel(6000 + 12 * 2500));
  TEST_ASSERT_EQUAL(1, member.listen_channel(6000 + 13 * 2500));
}

void test_a_member_stops_hunting_once_it_hears_a_heartbeat_and_hunts_again_after_six_more_seconds() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Member);
  units[0] = sls::GroupSync(kMacs[0], kGroupId, std::vector<sls::Mac>(kMacs, kMacs + 3), kSecret, sls::SyncRecord{sls::SyncRole::Coordinator}, kBootNonce,
                            sls::default_group_state());
  TEST_ASSERT_EQUAL(2, units[1].listen_channel(10000));

  deliver(units, units[0].on_tick(10000, 6), 10000);

  TEST_ASSERT_EQUAL(0, units[1].listen_channel(10000));
  TEST_ASSERT_EQUAL(0, units[1].listen_channel(15999));
  TEST_ASSERT_EQUAL(1, units[1].listen_channel(16000));
}

void test_an_undecided_unit_in_a_group_hunts_like_a_member() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Undecided);
  TEST_ASSERT_EQUAL(0, units[2].listen_channel(5999));
  TEST_ASSERT_EQUAL(2, units[2].listen_channel(8500));
}

void test_a_coordinator_never_hunts() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Coordinator);
  TEST_ASSERT_EQUAL(0, units[0].listen_channel(6000));
  TEST_ASSERT_EQUAL(0, units[0].listen_channel(100000));
}

void test_a_unit_alone_in_its_group_never_hunts() {
  sls::GroupSync alone(kMacs[1], kGroupId, std::vector<sls::Mac>{kMacs[1]}, kSecret, sls::SyncRecord{sls::SyncRole::Member}, kBootNonce, sls::default_group_state());
  TEST_ASSERT_EQUAL(0, alone.listen_channel(6000));
  TEST_ASSERT_EQUAL(0, alone.listen_channel(100000));
}

void test_an_undecided_unit_being_commissioned_never_moves_the_radio() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Undecided);
  units[2].on_commissioning(true);
  for (uint32_t now_ms = 6000; now_ms <= 6000 + 14 * 2500; now_ms += 2500) {
    TEST_ASSERT_EQUAL(0, units[2].listen_channel(now_ms));
  }
}

void test_an_undecided_unit_hunts_again_once_commissioning_stops() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Undecided);
  units[2].on_commissioning(true);
  units[2].on_commissioning(false);
  TEST_ASSERT_EQUAL(1, units[2].listen_channel(6000));
}

void test_an_undecided_unit_alone_in_its_group_never_becomes_a_member() {
  sls::GroupState initial = sls::default_group_state();
  sls::GroupState other = sls::default_group_state();
  other.open = !initial.open;
  sls::GroupSync alone(kMacs[0], kGroupId, std::vector<sls::Mac>{kMacs[0]}, kSecret, sls::SyncRecord{sls::SyncRole::Undecided}, kBootNonce, initial);
  sls::Message silent = signed_msg(sls::Message{sls::MessageType::MatterSilent, kGroupId, kMacs[1], {}});
  TEST_ASSERT_FALSE(alone.on_message(silent, kMacs[1], 1000));
  TEST_ASSERT_FALSE(alone.on_message(state_message(other, 1), kMacs[1], 1100));
  sls::Message heartbeat = signed_msg(sls::Message{sls::MessageType::Heartbeat, kGroupId, kMacs[1], sls::StateSync{other, 2, 0, 6}});
  TEST_ASSERT_FALSE(alone.on_message(heartbeat, kMacs[1], 1200));
  TEST_ASSERT_EQUAL(sls::SyncRole::Undecided, alone.role());
  TEST_ASSERT_TRUE(alone.matter_enabled());
  assert_same_state(initial, alone.state());
  TEST_ASSERT_EQUAL(0, alone.last_heard_ms());
}

void test_once_it_follows_a_coordinator_a_member_ignores_state_and_heartbeat_from_another_member() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupSync member(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member}, kBootNonce, sls::default_group_state());
  sls::GroupState coordinators = sls::default_group_state();
  coordinators.open = true;
  TEST_ASSERT_TRUE(member.on_message(signed_msg(sls::Message{sls::MessageType::Heartbeat, kGroupId, kMacs[0], sls::StateSync{coordinators, 5, 0, 6}}),
                                     kMacs[0], 1000));
  sls::GroupState other = coordinators;
  other.open = false;
  TEST_ASSERT_FALSE(member.on_message(signed_msg(sls::Message{sls::MessageType::State, kGroupId, kMacs[2], sls::StateSync{other, 9, 0, 0}}), kMacs[2], 2000));
  TEST_ASSERT_FALSE(member.on_message(signed_msg(sls::Message{sls::MessageType::Heartbeat, kGroupId, kMacs[2], sls::StateSync{other, 9, 0, 6}}), kMacs[2], 3000));
  assert_same_state(coordinators, member.state());
  TEST_ASSERT_EQUAL(1000, member.last_heard_ms());
  TEST_ASSERT_TRUE(member.on_message(signed_msg(sls::Message{sls::MessageType::Heartbeat, kGroupId, kMacs[0], sls::StateSync{other, 5, 0, 6}}), kMacs[0], 4000));
}

void test_a_sync_message_with_a_wrong_or_missing_tag_is_ignored() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Undecided);
  sls::GroupState changed = sls::apply(sls::default_group_state(), sls::Command::open());
  const sls::Message silent{sls::MessageType::MatterSilent, kGroupId, kMacs[0], std::monostate{}};
  const sls::Message state{sls::MessageType::State, kGroupId, kMacs[0], sls::StateSync{changed, 7, 0, 6}};
  const sls::Message heartbeat{sls::MessageType::Heartbeat, kGroupId, kMacs[0], sls::StateSync{changed, 8, 0, 6}};

  for (const sls::Message& base : {silent, state, heartbeat}) {
    TEST_ASSERT_FALSE(units[1].on_message(signed_msg(base, kOtherSecret), kMacs[0], 1000));  // signed with another secret
    TEST_ASSERT_FALSE(units[1].on_message(base, kMacs[0], 1100));                            // all-zero tag
  }
  TEST_ASSERT_FALSE(units[1].state().open);
  TEST_ASSERT_EQUAL(static_cast<int>(sls::SyncRole::Undecided), static_cast<int>(units[1].role()));
  TEST_ASSERT_TRUE(units[1].matter_enabled());
  TEST_ASSERT_EQUAL(0, units[1].last_heard_ms());
}

void test_a_forged_matter_silent_neither_settles_an_undecided_unit_nor_pins_the_claimed_sender() {
  std::vector<sls::GroupSync> units = make_units(sls::SyncRole::Undecided);
  const sls::Message forged = signed_msg(sls::Message{sls::MessageType::MatterSilent, kGroupId, kMacs[2], std::monostate{}}, kOtherSecret);

  TEST_ASSERT_FALSE(units[1].on_message(forged, kMacs[2], 1000));
  TEST_ASSERT_EQUAL(static_cast<int>(sls::SyncRole::Undecided), static_cast<int>(units[1].role()));

  const sls::Message genuine = signed_msg(sls::Message{sls::MessageType::MatterSilent, kGroupId, kMacs[0], std::monostate{}});
  TEST_ASSERT_TRUE(units[1].on_message(genuine, kMacs[0], 2000));
  TEST_ASSERT_EQUAL(static_cast<int>(sls::SyncRole::Member), static_cast<int>(units[1].role()));
}

static sls::Message heartbeat_stamped(const sls::GroupState& state, uint32_t nonce, uint32_t counter) {
  sls::Message m{sls::MessageType::Heartbeat, kGroupId, kMacs[0], sls::StateSync{state, 1, 0, 6}};
  m.auth.boot_nonce = nonce;
  m.auth.counter = counter;
  return signed_msg(m);
}

static sls::Message state_stamped(const sls::GroupState& state, uint32_t seq, uint32_t nonce, uint32_t counter) {
  sls::Message m{sls::MessageType::State, kGroupId, kMacs[0], sls::StateSync{state, seq, 0, 0}};
  m.auth.boot_nonce = nonce;
  m.auth.counter = counter;
  return signed_msg(m);
}

void test_a_replayed_heartbeat_is_ignored() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupState initial = sls::default_group_state();
  sls::GroupSync member(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member}, kBootNonce, initial);
  sls::GroupState a = sls::apply(initial, sls::Command::open());
  sls::GroupState b = sls::apply(a, sls::Command::set_hue_sat(200, 80));
  const sls::Message h1 = heartbeat_stamped(a, kBootNonce, 5);
  const sls::Message h2 = heartbeat_stamped(b, kBootNonce, 6);
  member.on_message(h1, kMacs[0], 1000);
  member.on_message(h2, kMacs[0], 2000);

  const bool replay1 = member.on_message(h1, kMacs[0], 3000);
  const bool replay2 = member.on_message(h2, kMacs[0], 4000);

  TEST_ASSERT_FALSE(replay1);
  TEST_ASSERT_FALSE(replay2);
  assert_same_state(b, member.state());
  TEST_ASSERT_EQUAL_UINT32(2000, member.last_heard_ms());
}

void test_a_replayed_state_is_ignored() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupState initial = sls::default_group_state();
  sls::GroupSync member(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member}, kBootNonce, initial);
  sls::GroupState a = sls::apply(initial, sls::Command::open());
  sls::GroupState b = sls::apply(a, sls::Command::set_hue_sat(200, 80));
  const sls::Message s1 = state_stamped(a, 1, kBootNonce, 5);
  const sls::Message s2 = state_stamped(b, 2, kBootNonce, 6);
  member.on_message(s1, kMacs[0], 1000);
  member.on_message(s2, kMacs[0], 2000);

  const bool replay = member.on_message(s1, kMacs[0], 3000);

  TEST_ASSERT_FALSE(replay);
  assert_same_state(b, member.state());
  TEST_ASSERT_EQUAL_UINT32(2000, member.last_heard_ms());
}

void test_a_member_follows_a_coordinator_that_rebooted_with_a_new_boot_nonce() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupState initial = sls::default_group_state();
  sls::GroupSync member(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member}, kBootNonce, initial);
  member.on_message(heartbeat_stamped(initial, kBootNonce, 50), kMacs[0], 1000);
  sls::GroupSync rebooted(kMacs[0], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Coordinator}, 0x9999, initial);
  sls::GroupState changed = sls::apply(initial, sls::Command::set_hue_sat(200, 80));
  rebooted.on_local_change(changed, 4000);

  bool accepted = false;
  for (const sls::Outgoing& o : rebooted.on_tick(5000, 6))
    if (o.to == kMacs[1]) accepted = member.on_message(o.message, o.message.sender, 5000);

  TEST_ASSERT_TRUE(accepted);
  assert_same_state(changed, member.state());
}

void test_frames_from_a_coordinators_earlier_boot_are_ignored_once_it_rebooted() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupState initial = sls::default_group_state();
  sls::GroupSync member(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member}, kBootNonce, initial);
  sls::GroupState a = sls::apply(initial, sls::Command::open());
  sls::GroupState b = sls::apply(a, sls::Command::set_hue_sat(200, 80));
  member.on_message(heartbeat_stamped(a, kBootNonce, 5), kMacs[0], 1000);
  member.on_message(heartbeat_stamped(b, 0x9999, 1), kMacs[0], 2000);

  const bool captured = member.on_message(heartbeat_stamped(a, kBootNonce, 900), kMacs[0], 3000);

  TEST_ASSERT_FALSE(captured);
  assert_same_state(b, member.state());
  TEST_ASSERT_EQUAL_UINT32(2000, member.last_heard_ms());
}

void test_re_pairing_clears_a_members_role_and_coordinator_pin() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupSync member(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member, kMacs[0]}, kBootNonce, sls::default_group_state());
  member.regroup(kGroupId, table, kOtherSecret);
  TEST_ASSERT_EQUAL(sls::SyncRole::Undecided, member.role());
  TEST_ASSERT_TRUE(member.matter_enabled());
  TEST_ASSERT_EQUAL(sls::SyncRole::Undecided, member.record().role);
  TEST_ASSERT_FALSE(member.record().coordinator.has_value());
}

void test_re_pairing_clears_a_coordinators_role() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupSync coordinator(kMacs[0], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Coordinator}, kBootNonce, sls::default_group_state());
  coordinator.regroup(kGroupId, table, kOtherSecret);
  TEST_ASSERT_EQUAL(sls::SyncRole::Undecided, coordinator.record().role);
  TEST_ASSERT_TRUE(coordinator.on_tick(10000, 6).empty());
}

void test_after_re_pairing_only_frames_signed_with_the_new_group_secret_are_heard() {
  std::vector<sls::Mac> table(kMacs, kMacs + 3);
  sls::GroupSync member(kMacs[1], kGroupId, table, kSecret, sls::SyncRecord{sls::SyncRole::Member, kMacs[0]}, kBootNonce, sls::default_group_state());
  sls::Message old_frame = signed_msg(sls::Message{sls::MessageType::MatterSilent, kGroupId, kMacs[2], std::monostate{}}, kSecret);
  member.on_message(signed_msg(sls::Message{sls::MessageType::MatterSilent, kGroupId, kMacs[0], std::monostate{}}), kMacs[0], 100);
  member.regroup(kGroupId, table, kOtherSecret);
  TEST_ASSERT_FALSE(member.on_message(old_frame, kMacs[2], 200));
  TEST_ASSERT_EQUAL(sls::SyncRole::Undecided, member.role());
  sls::Message fresh;
  fresh.type = sls::MessageType::MatterSilent;
  fresh.group_id = kGroupId;
  fresh.sender = kMacs[2];
  fresh.body = std::monostate{};
  fresh.auth.boot_nonce = kBootNonce;
  fresh.auth.counter = 1;
  sls::sign(fresh, kOtherSecret);
  TEST_ASSERT_TRUE(member.on_message(fresh, kMacs[2], 300));
  TEST_ASSERT_EQUAL(sls::SyncRole::Member, member.record().role);
  TEST_ASSERT_TRUE(member.record().coordinator == std::optional<sls::Mac>(kMacs[2]));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_a_sync_message_with_a_wrong_or_missing_tag_is_ignored);
  RUN_TEST(test_a_forged_matter_silent_neither_settles_an_undecided_unit_nor_pins_the_claimed_sender);
  RUN_TEST(test_once_it_follows_a_coordinator_a_member_ignores_state_and_heartbeat_from_another_member);
  RUN_TEST(test_a_member_that_hears_nothing_hunts_across_the_channels_after_six_seconds);
  RUN_TEST(test_the_coordinator_ignores_states_and_heartbeats_it_receives);
  RUN_TEST(test_a_local_change_equal_to_the_current_state_is_a_no_op);
  RUN_TEST(test_an_undecided_unit_applies_a_local_change_but_relays_nothing);
  RUN_TEST(test_a_stale_state_still_counts_as_hearing_the_coordinator);
  RUN_TEST(test_the_first_commissioned_unit_becomes_coordinator_and_silences_the_others);
  RUN_TEST(test_a_coordinator_stays_coordinator_when_it_hears_matter_silent);
  RUN_TEST(test_a_persisted_member_is_matter_silent_from_boot);
  RUN_TEST(test_a_persisted_coordinator_sends_heartbeats_without_being_commissioned_again);
  RUN_TEST(test_a_member_never_relays_a_local_change);
  RUN_TEST(test_a_unit_ignores_messages_from_another_group);
  RUN_TEST(test_a_sync_message_from_a_sender_outside_the_group_is_ignored);
  RUN_TEST(test_a_sync_message_whose_source_differs_from_its_claimed_sender_is_ignored);
  RUN_TEST(test_a_member_realigns_its_blink_clock_when_drift_exceeds_the_threshold);
  RUN_TEST(test_a_member_leaves_its_blink_clock_alone_when_drift_is_within_the_threshold);
  RUN_TEST(test_blink_drift_is_measured_around_the_period);
  RUN_TEST(test_a_member_blinks_in_step_with_the_coordinator_after_the_first_heartbeat);
  RUN_TEST(test_a_member_that_missed_a_state_catches_up_on_the_next_heartbeat);
  RUN_TEST(test_a_member_follows_a_rebooted_coordinator_after_its_next_heartbeat);
  RUN_TEST(test_a_rebooted_member_catches_up_on_the_next_heartbeat);
  RUN_TEST(test_the_coordinator_sends_a_heartbeat_to_every_other_member_every_two_seconds);
  RUN_TEST(test_a_member_or_undecided_unit_sends_no_heartbeat);
  RUN_TEST(test_a_member_hearing_a_heartbeat_records_when);
  RUN_TEST(test_an_undecided_unit_that_hears_a_heartbeat_becomes_a_member);
  RUN_TEST(test_an_undecided_unit_that_hears_a_state_becomes_a_member);
  RUN_TEST(test_a_change_on_the_coordinator_reaches_every_member);
  RUN_TEST(test_a_member_drops_a_state_with_an_older_sequence);
  RUN_TEST(test_a_member_accepts_a_newer_sequence_across_32_bit_wrap_around);
  RUN_TEST(test_a_member_stops_hunting_once_it_hears_a_heartbeat_and_hunts_again_after_six_more_seconds);
  RUN_TEST(test_an_undecided_unit_in_a_group_hunts_like_a_member);
  RUN_TEST(test_a_coordinator_never_hunts);
  RUN_TEST(test_a_unit_alone_in_its_group_never_hunts);
  RUN_TEST(test_an_undecided_unit_being_commissioned_never_moves_the_radio);
  RUN_TEST(test_an_undecided_unit_hunts_again_once_commissioning_stops);
  RUN_TEST(test_an_undecided_unit_alone_in_its_group_never_becomes_a_member);
  RUN_TEST(test_a_replayed_heartbeat_is_ignored);
  RUN_TEST(test_a_members_record_names_the_coordinator_it_learned_from_an_authenticated_matter_silent);
  RUN_TEST(test_an_undecided_unit_that_only_got_a_forged_frame_has_no_coordinator_in_its_record);
  RUN_TEST(test_a_coordinators_record_has_no_coordinator_to_follow);
  RUN_TEST(test_the_coordinator_pin_survives_a_restart);
  RUN_TEST(test_a_replayed_state_is_ignored);
  RUN_TEST(test_a_member_follows_a_coordinator_that_rebooted_with_a_new_boot_nonce);
  RUN_TEST(test_frames_from_a_coordinators_earlier_boot_are_ignored_once_it_rebooted);
  RUN_TEST(test_re_pairing_clears_a_members_role_and_coordinator_pin);
  RUN_TEST(test_re_pairing_clears_a_coordinators_role);
  RUN_TEST(test_after_re_pairing_only_frames_signed_with_the_new_group_secret_are_heard);
  return UNITY_END();
}
