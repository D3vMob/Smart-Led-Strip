// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#include <unity.h>
#include <algorithm>
#include <vector>
#include "pairing.h"
#include "sync_auth.h"

void setUp() {}
void tearDown() {}

static const sls::Mac kSelf = {0x02, 0, 0, 0, 0, 0x01};
static const sls::Mac kPeer = {0x02, 0, 0, 0, 0, 0x05};
static const sls::Mac kLowPeer = {0x02, 0, 0, 0, 0, 0x00};

static sls::PairingNonce nonce_of(uint8_t seed) {
  sls::PairingNonce nonce;
  for (size_t i = 0; i < nonce.size(); ++i) nonce[i] = uint8_t(seed + i);
  return nonce;
}

static bool is_zero(const sls::GroupSecret& secret) {
  return std::all_of(secret.begin(), secret.end(), [](uint8_t b) { return b == 0; });
}

static sls::PairingInfo solo(const sls::Mac& peer, uint32_t window_age_ms = 0) {
  return sls::PairingInfo{window_age_ms, {peer}};
}

// Delivers each Unit's hellos to every other Unit with an open window, and
// each ack back to the Unit it is addressed to.
struct Radio {
  std::vector<sls::Pairing> units;
  uint8_t nonces_shift = 0;  // moves every Unit's nonce, to vary a scenario

  void deliver(size_t from, const sls::PairingActions& actions, uint32_t now_ms) {
    if (!actions.send_hello) return;
    for (size_t to = 0; to < units.size(); ++to) {
      if (to == from || !units[to].window_open()) continue;
      sls::PairingActions reply = units[to].on_hello(mac_of(from), units[from].pairing_info(now_ms), now_ms);
      if (reply.send_ack_to) units[from].on_ack(mac_of(to), units[to].pairing_info(now_ms), now_ms);
    }
  }

  std::vector<sls::Mac> macs;
  const sls::Mac& mac_of(size_t i) const { return macs[i]; }

  Radio(const std::vector<sls::Mac>& m) : macs(m) {
    for (const sls::Mac& mac : m) units.emplace_back(mac);
  }

  void run(const std::vector<uint32_t>& presses, uint32_t until_ms) {
    for (uint32_t now = 0; now <= until_ms; now += 100) {
      for (size_t i = 0; i < units.size(); ++i) {
        if (now == presses[i]) deliver(i, units[i].on_long_press(now, nonce_of(uint8_t(16 * (i + 1) + nonces_shift))), now);
        deliver(i, units[i].on_tick(now), now);
      }
    }
  }
};

void test_long_press_opens_the_window_and_broadcasts_a_hello_right_away() {
  sls::Pairing pairing(kSelf);
  TEST_ASSERT_FALSE(pairing.window_open());

  sls::PairingActions actions = pairing.on_long_press(1000, nonce_of(1));

  TEST_ASSERT_TRUE(actions.window_open);
  TEST_ASSERT_TRUE(actions.send_hello);
  TEST_ASSERT_TRUE(pairing.window_open());
}

void test_window_closes_30_seconds_after_the_press() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000, nonce_of(1));

  TEST_ASSERT_TRUE(pairing.on_tick(30999).window_open);
  sls::PairingActions closed = pairing.on_tick(31000);

  TEST_ASSERT_FALSE(closed.window_open);
  TEST_ASSERT_FALSE(pairing.window_open());
}

void test_second_press_extends_the_window() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000, nonce_of(1));
  pairing.on_long_press(20000, nonce_of(1));

  TEST_ASSERT_TRUE(pairing.on_tick(31000).window_open);
  TEST_ASSERT_FALSE(pairing.on_tick(50000).window_open);
}

void test_hello_is_sent_once_per_interval_while_the_window_is_open() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000, nonce_of(1));

  TEST_ASSERT_FALSE(pairing.on_tick(1500).send_hello);
  TEST_ASSERT_TRUE(pairing.on_tick(2000).send_hello);
  TEST_ASSERT_FALSE(pairing.on_tick(2500).send_hello);
  TEST_ASSERT_TRUE(pairing.on_tick(3000).send_hello);
}

void test_no_hello_is_sent_after_the_window_closes() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000, nonce_of(1));

  TEST_ASSERT_FALSE(pairing.on_tick(31000).send_hello);
  TEST_ASSERT_FALSE(pairing.on_tick(32000).send_hello);
}

void test_fresh_pairing_is_a_group_of_one() {
  sls::Pairing pairing(kSelf);
  TEST_ASSERT_EQUAL_UINT(1, pairing.members().size());
  TEST_ASSERT_TRUE(kSelf == pairing.members().front());
}

void test_hello_from_a_new_peer_adds_it_and_is_acked() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000, nonce_of(1));

  sls::PairingActions actions = pairing.on_hello(kPeer, solo(kPeer), 1100);

  TEST_ASSERT_EQUAL_UINT(2, pairing.members().size());
  TEST_ASSERT_TRUE(actions.send_ack_to.has_value());
  TEST_ASSERT_TRUE(kPeer == *actions.send_ack_to);
}

void test_second_hello_from_the_same_peer_adds_nothing_and_is_not_acked() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000, nonce_of(1));
  pairing.on_hello(kPeer, solo(kPeer), 1100);

  sls::PairingActions actions = pairing.on_hello(kPeer, solo(kPeer), 2100);

  TEST_ASSERT_EQUAL_UINT(2, pairing.members().size());
  TEST_ASSERT_FALSE(actions.send_ack_to.has_value());
}

void test_ack_from_a_new_peer_adds_it_without_replying_and_members_stay_sorted() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000, nonce_of(1));
  pairing.on_hello(kPeer, solo(kPeer), 1100);

  sls::PairingActions actions = pairing.on_ack(kLowPeer, solo(kLowPeer), 1200);

  TEST_ASSERT_FALSE(actions.send_ack_to.has_value());
  TEST_ASSERT_EQUAL_UINT(3, pairing.members().size());
  TEST_ASSERT_TRUE(kLowPeer == pairing.members()[0]);
  TEST_ASSERT_TRUE(kSelf == pairing.members()[1]);
  TEST_ASSERT_TRUE(kPeer == pairing.members()[2]);
}

void test_group_id_is_fnv1a_of_the_lowest_member_mac() {
  sls::Pairing pairing(kSelf);
  TEST_ASSERT_EQUAL_HEX32(0x772375FC, pairing.group_id());

  pairing.on_long_press(1000, nonce_of(1));
  pairing.on_hello(kLowPeer, solo(kLowPeer), 1100);
  TEST_ASSERT_EQUAL_HEX32(0x7823778F, pairing.group_id());
}

void test_group_id_is_never_zero() {
  const sls::Mac zero_hash_mac = {0x02, 0x06, 0x93, 0x8A, 0xC8, 0x9C};
  sls::Pairing pairing(zero_hash_mac);
  TEST_ASSERT_EQUAL_HEX32(1, pairing.group_id());
}

void test_two_units_pressed_at_different_times_converge_on_one_group() {
  Radio radio({{0x02, 0, 0, 0, 0, 0x09}, {0x02, 0, 0, 0, 0, 0x03}});
  radio.run({1000, 5000}, 40000);

  TEST_ASSERT_EQUAL_UINT(2, radio.units[0].members().size());
  TEST_ASSERT_TRUE(radio.units[0].members() == radio.units[1].members());
  TEST_ASSERT_EQUAL_HEX32(radio.units[0].group_id(), radio.units[1].group_id());
}

void test_three_units_pressed_at_different_times_converge_on_one_group() {
  Radio radio({{0x02, 0, 0, 0, 0, 0x09}, {0x02, 0, 0, 0, 0, 0x05}, {0x02, 0, 0, 0, 0, 0x01}});
  radio.run({1000, 10000, 20000}, 60000);

  TEST_ASSERT_EQUAL_UINT(3, radio.units[0].members().size());
  TEST_ASSERT_TRUE(radio.units[0].members() == radio.units[1].members());
  TEST_ASSERT_TRUE(radio.units[0].members() == radio.units[2].members());
  TEST_ASSERT_EQUAL_HEX32(radio.units[0].group_id(), radio.units[1].group_id());
  TEST_ASSERT_EQUAL_HEX32(radio.units[0].group_id(), radio.units[2].group_id());
}

void test_group_formed_is_reported_once_when_the_window_closes() {
  sls::Pairing pairing(kSelf);
  TEST_ASSERT_FALSE(pairing.on_long_press(1000, nonce_of(1)).group_formed);

  TEST_ASSERT_FALSE(pairing.on_tick(30999).group_formed);
  TEST_ASSERT_TRUE(pairing.on_tick(31000).group_formed);
  TEST_ASSERT_FALSE(pairing.on_tick(31100).group_formed);
  TEST_ASSERT_FALSE(pairing.on_tick(60000).group_formed);
}

void test_ninth_hello_is_refused_without_evicting_anyone() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000, nonce_of(1));
  for (uint8_t i = 0; i < 7; ++i) {
    const sls::Mac peer = {0x02, 0, 0, 0, 0, uint8_t(0x10 + i)};
    pairing.on_hello(peer, solo(peer), 1100);
  }
  TEST_ASSERT_EQUAL_UINT(8, pairing.members().size());

  sls::PairingActions actions = pairing.on_hello(kLowPeer, solo(kLowPeer), 1200);

  TEST_ASSERT_EQUAL_UINT(8, pairing.members().size());
  TEST_ASSERT_TRUE(kSelf == pairing.members().front());
  TEST_ASSERT_FALSE(actions.send_ack_to.has_value());
}

void test_ninth_ack_is_ignored() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000, nonce_of(1));
  for (uint8_t i = 0; i < 7; ++i) {
    const sls::Mac peer = {0x02, 0, 0, 0, 0, uint8_t(0x10 + i)};
    pairing.on_hello(peer, solo(peer), 1100);
  }

  pairing.on_ack(kLowPeer, solo(kLowPeer), 1200);

  TEST_ASSERT_EQUAL_UINT(8, pairing.members().size());
  TEST_ASSERT_TRUE(kSelf == pairing.members().front());
}

void test_hello_and_ack_after_the_window_closes_are_ignored() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000, nonce_of(1));
  pairing.on_tick(31000);

  sls::PairingActions hello = pairing.on_hello(kPeer, solo(kPeer), 32000);
  pairing.on_ack(kLowPeer, solo(kLowPeer), 32100);

  TEST_ASSERT_FALSE(hello.send_ack_to.has_value());
  TEST_ASSERT_FALSE(pairing.window_open());
  TEST_ASSERT_EQUAL_UINT(1, pairing.members().size());
}

void test_hello_before_any_press_is_ignored() {
  sls::Pairing pairing(kSelf);

  sls::PairingActions hello = pairing.on_hello(kPeer, solo(kPeer), 500);

  TEST_ASSERT_FALSE(hello.send_ack_to.has_value());
  TEST_ASSERT_FALSE(pairing.window_open());
  TEST_ASSERT_EQUAL_UINT(1, pairing.members().size());
}

void test_unit_with_no_peers_closes_as_a_group_of_one() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000, nonce_of(1));

  sls::PairingActions closing = pairing.on_tick(31000);

  TEST_ASSERT_TRUE(closing.group_formed);
  TEST_ASSERT_EQUAL_UINT(1, pairing.members().size());
  TEST_ASSERT_TRUE(kSelf == pairing.members().front());
  TEST_ASSERT_EQUAL_HEX32(0x772375FC, pairing.group_id());
}

void test_new_long_press_starts_pairing_fresh_as_a_group_of_one() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000, nonce_of(1));
  pairing.on_hello(kPeer, solo(kPeer), 1100);
  pairing.on_tick(31000);
  TEST_ASSERT_EQUAL_UINT(2, pairing.members().size());

  pairing.on_long_press(40000, nonce_of(1));

  TEST_ASSERT_EQUAL_UINT(1, pairing.members().size());
  TEST_ASSERT_TRUE(kSelf == pairing.members().front());
  TEST_ASSERT_TRUE(pairing.on_tick(70000).group_formed);
  TEST_ASSERT_EQUAL_UINT(1, pairing.members().size());
}

void test_restored_group_has_the_persisted_table_and_the_same_group_id() {
  Radio radio({{0x02, 0, 0, 0, 0, 0x09}, {0x02, 0, 0, 0, 0, 0x05}, {0x02, 0, 0, 0, 0, 0x01}});
  radio.run({1000, 10000, 20000}, 60000);
  const sls::Mac self = radio.macs[0];
  std::vector<sls::Mac> persisted = {radio.macs[2], radio.macs[1]};

  sls::Pairing restored(self, persisted, sls::GroupSecret{});

  TEST_ASSERT_FALSE(restored.window_open());
  TEST_ASSERT_TRUE(radio.units[0].members() == restored.members());
  TEST_ASSERT_EQUAL_HEX32(radio.units[0].group_id(), restored.group_id());
}

void test_restored_group_is_deduplicated_and_capped_at_eight() {
  std::vector<sls::Mac> persisted = {kSelf, kPeer, kPeer};
  for (uint8_t i = 0; i < 10; ++i) {
    persisted.push_back({0x02, 0, 0, 0, 0, uint8_t(0x10 + i)});
  }

  sls::Pairing restored(kSelf, persisted, sls::GroupSecret{});

  TEST_ASSERT_EQUAL_UINT(8, restored.members().size());
  TEST_ASSERT_TRUE(kSelf == restored.members().front());
  TEST_ASSERT_TRUE(restored.members() == [&] { auto m = restored.members(); std::sort(m.begin(), m.end()); return m; }());
  TEST_ASSERT_FALSE(restored.window_open());
}

void ninth_unit_checks(const std::vector<uint32_t>& presses, const std::vector<sls::Mac>& macs, size_t refused) {
  Radio radio(macs);
  radio.run(presses, 80000);

  TEST_ASSERT_EQUAL_UINT(1, radio.units[refused].members().size());
  TEST_ASSERT_TRUE(macs[refused] == radio.units[refused].members().front());
  const size_t first = refused == 0 ? 1 : 0;
  for (size_t i = 0; i < macs.size(); ++i) {
    if (i == refused) continue;
    TEST_ASSERT_EQUAL_UINT(8, radio.units[i].members().size());
    TEST_ASSERT_TRUE(radio.units[first].members() == radio.units[i].members());
    TEST_ASSERT_EQUAL_HEX32(radio.units[first].group_id(), radio.units[i].group_id());
    TEST_ASSERT_TRUE(std::find(radio.units[i].members().begin(), radio.units[i].members().end(), macs[refused]) ==
                     radio.units[i].members().end());
  }
}

void test_nine_units_pressed_at_once_refuse_the_highest_mac() {
  std::vector<sls::Mac> macs;
  for (uint8_t i = 0; i < 9; ++i) macs.push_back({0x02, 0, 0, 0, 0, uint8_t(0x20 + i)});
  ninth_unit_checks(std::vector<uint32_t>(9, 1000), macs, 8);
}

void test_nine_units_pressed_one_after_another_refuse_the_last_pressed() {
  std::vector<sls::Mac> macs;
  for (uint8_t i = 0; i < 8; ++i) macs.push_back({0x02, 0, 0, 0, 0, uint8_t(0x20 + i)});
  macs.push_back({0x02, 0, 0, 0, 0, 0x01});
  std::vector<uint32_t> presses;
  for (uint32_t i = 0; i < 9; ++i) presses.push_back(1000 + i * 1000);
  ninth_unit_checks(presses, macs, 8);
}

void test_ninth_unit_pressed_after_the_first_window_closed_is_refused() {
  std::vector<sls::Mac> macs;
  for (uint8_t i = 0; i < 8; ++i) macs.push_back({0x02, 0, 0, 0, 0, uint8_t(0x20 + i)});
  macs.push_back({0x02, 0, 0, 0, 0, 0x01});
  std::vector<uint32_t> presses{0};
  for (uint32_t i = 0; i < 7; ++i) presses.push_back(10000 + i * 1000);
  presses.push_back(31000);
  ninth_unit_checks(presses, macs, 8);
}

void test_two_extra_units_stay_out_of_the_group_and_pair_with_each_other() {
  std::vector<sls::Mac> macs;
  for (uint8_t i = 0; i < 10; ++i) macs.push_back({0x02, 0, 0, 0, 0, uint8_t(0x20 + i)});
  std::vector<uint32_t> presses;
  for (uint32_t i = 0; i < 10; ++i) presses.push_back(1000 + i * 1000);
  Radio radio(macs);
  radio.run(presses, 80000);

  for (size_t i = 0; i < 8; ++i) {
    TEST_ASSERT_EQUAL_UINT(8, radio.units[i].members().size());
    TEST_ASSERT_TRUE(radio.units[0].members() == radio.units[i].members());
    TEST_ASSERT_EQUAL_HEX32(radio.units[0].group_id(), radio.units[i].group_id());
  }
  const std::vector<sls::Mac> extras = {macs[8], macs[9]};
  TEST_ASSERT_TRUE(extras == radio.units[8].members());
  TEST_ASSERT_TRUE(extras == radio.units[9].members());
  TEST_ASSERT_NOT_EQUAL(radio.units[0].group_id(), radio.units[8].group_id());
}

static sls::GroupSecret formed_secret(uint8_t nonce_shift, size_t unit, size_t units) {
  std::vector<sls::Mac> macs = {{0x02, 0, 0, 0, 0, 0x01}, {0x02, 0, 0, 0, 0, 0x02}, {0x02, 0, 0, 0, 0, 0x03}};
  macs.resize(units);
  Radio radio(macs);
  radio.nonces_shift = nonce_shift;
  radio.run(std::vector<uint32_t>(units, 1000), 40000);
  return radio.units[unit].group_secret();
}

void test_every_member_of_a_formed_group_derives_the_same_group_secret() {
  const sls::GroupSecret first = formed_secret(0, 0, 3);
  TEST_ASSERT_FALSE(is_zero(first));
  TEST_ASSERT_TRUE(first == formed_secret(0, 1, 3));
  TEST_ASSERT_TRUE(first == formed_secret(0, 2, 3));
}

void test_the_group_secret_depends_on_every_members_nonce() {
  TEST_ASSERT_FALSE(formed_secret(0, 0, 3) == formed_secret(1, 0, 3));
}

void test_a_restored_group_keeps_its_secret() {
  sls::GroupSecret secret;
  for (size_t i = 0; i < secret.size(); ++i) secret[i] = uint8_t(i + 1);
  sls::Pairing restored(kSelf, {kSelf, kPeer}, secret);
  TEST_ASSERT_TRUE(secret == restored.group_secret());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_long_press_opens_the_window_and_broadcasts_a_hello_right_away);
  RUN_TEST(test_window_closes_30_seconds_after_the_press);
  RUN_TEST(test_second_press_extends_the_window);
  RUN_TEST(test_hello_is_sent_once_per_interval_while_the_window_is_open);
  RUN_TEST(test_no_hello_is_sent_after_the_window_closes);
  RUN_TEST(test_fresh_pairing_is_a_group_of_one);
  RUN_TEST(test_hello_from_a_new_peer_adds_it_and_is_acked);
  RUN_TEST(test_second_hello_from_the_same_peer_adds_nothing_and_is_not_acked);
  RUN_TEST(test_ack_from_a_new_peer_adds_it_without_replying_and_members_stay_sorted);
  RUN_TEST(test_group_id_is_fnv1a_of_the_lowest_member_mac);
  RUN_TEST(test_group_id_is_never_zero);
  RUN_TEST(test_two_units_pressed_at_different_times_converge_on_one_group);
  RUN_TEST(test_three_units_pressed_at_different_times_converge_on_one_group);
  RUN_TEST(test_group_formed_is_reported_once_when_the_window_closes);
  RUN_TEST(test_ninth_hello_is_refused_without_evicting_anyone);
  RUN_TEST(test_ninth_ack_is_ignored);
  RUN_TEST(test_hello_and_ack_after_the_window_closes_are_ignored);
  RUN_TEST(test_hello_before_any_press_is_ignored);
  RUN_TEST(test_unit_with_no_peers_closes_as_a_group_of_one);
  RUN_TEST(test_new_long_press_starts_pairing_fresh_as_a_group_of_one);
  RUN_TEST(test_restored_group_has_the_persisted_table_and_the_same_group_id);
  RUN_TEST(test_every_member_of_a_formed_group_derives_the_same_group_secret);
  RUN_TEST(test_the_group_secret_depends_on_every_members_nonce);
  RUN_TEST(test_a_restored_group_keeps_its_secret);
  RUN_TEST(test_restored_group_is_deduplicated_and_capped_at_eight);
  RUN_TEST(test_nine_units_pressed_at_once_refuse_the_highest_mac);
  RUN_TEST(test_nine_units_pressed_one_after_another_refuse_the_last_pressed);
  RUN_TEST(test_ninth_unit_pressed_after_the_first_window_closed_is_refused);
  RUN_TEST(test_two_extra_units_stay_out_of_the_group_and_pair_with_each_other);
  return UNITY_END();
}
