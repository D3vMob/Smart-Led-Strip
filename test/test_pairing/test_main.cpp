#include <unity.h>
#include <algorithm>
#include <vector>
#include "pairing.h"

void setUp() {}
void tearDown() {}

static const sls::Mac kSelf = {0x02, 0, 0, 0, 0, 0x01};
static const sls::Mac kPeer = {0x02, 0, 0, 0, 0, 0x05};
static const sls::Mac kLowPeer = {0x02, 0, 0, 0, 0, 0x00};

// Delivers each Unit's hellos to every other Unit with an open window, and
// each ack back to the Unit it is addressed to.
struct Radio {
  std::vector<sls::Pairing> units;

  void deliver(size_t from, const sls::PairingActions& actions, uint32_t now_ms) {
    if (!actions.send_hello) return;
    for (size_t to = 0; to < units.size(); ++to) {
      if (to == from || !units[to].window_open()) continue;
      sls::PairingActions reply = units[to].on_hello(mac_of(from), now_ms);
      if (reply.send_ack_to) units[from].on_ack(mac_of(to), now_ms);
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
        if (now == presses[i]) deliver(i, units[i].on_long_press(now), now);
        deliver(i, units[i].on_tick(now), now);
      }
    }
  }
};

void test_long_press_opens_the_window_and_broadcasts_a_hello_right_away() {
  sls::Pairing pairing(kSelf);
  TEST_ASSERT_FALSE(pairing.window_open());

  sls::PairingActions actions = pairing.on_long_press(1000);

  TEST_ASSERT_TRUE(actions.window_open);
  TEST_ASSERT_TRUE(actions.send_hello);
  TEST_ASSERT_TRUE(pairing.window_open());
}

void test_window_closes_30_seconds_after_the_press() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000);

  TEST_ASSERT_TRUE(pairing.on_tick(30999).window_open);
  sls::PairingActions closed = pairing.on_tick(31000);

  TEST_ASSERT_FALSE(closed.window_open);
  TEST_ASSERT_FALSE(pairing.window_open());
}

void test_second_press_extends_the_window() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000);
  pairing.on_long_press(20000);

  TEST_ASSERT_TRUE(pairing.on_tick(31000).window_open);
  TEST_ASSERT_FALSE(pairing.on_tick(50000).window_open);
}

void test_hello_is_sent_once_per_interval_while_the_window_is_open() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000);

  TEST_ASSERT_FALSE(pairing.on_tick(1500).send_hello);
  TEST_ASSERT_TRUE(pairing.on_tick(2000).send_hello);
  TEST_ASSERT_FALSE(pairing.on_tick(2500).send_hello);
  TEST_ASSERT_TRUE(pairing.on_tick(3000).send_hello);
}

void test_no_hello_is_sent_after_the_window_closes() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000);

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
  pairing.on_long_press(1000);

  sls::PairingActions actions = pairing.on_hello(kPeer, 1100);

  TEST_ASSERT_EQUAL_UINT(2, pairing.members().size());
  TEST_ASSERT_TRUE(actions.send_ack_to.has_value());
  TEST_ASSERT_TRUE(kPeer == *actions.send_ack_to);
}

void test_second_hello_from_the_same_peer_adds_nothing_and_is_not_acked() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000);
  pairing.on_hello(kPeer, 1100);

  sls::PairingActions actions = pairing.on_hello(kPeer, 2100);

  TEST_ASSERT_EQUAL_UINT(2, pairing.members().size());
  TEST_ASSERT_FALSE(actions.send_ack_to.has_value());
}

void test_ack_from_a_new_peer_adds_it_without_replying_and_members_stay_sorted() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000);
  pairing.on_hello(kPeer, 1100);

  sls::PairingActions actions = pairing.on_ack(kLowPeer, 1200);

  TEST_ASSERT_FALSE(actions.send_ack_to.has_value());
  TEST_ASSERT_EQUAL_UINT(3, pairing.members().size());
  TEST_ASSERT_TRUE(kLowPeer == pairing.members()[0]);
  TEST_ASSERT_TRUE(kSelf == pairing.members()[1]);
  TEST_ASSERT_TRUE(kPeer == pairing.members()[2]);
}

void test_group_id_is_fnv1a_of_the_lowest_member_mac() {
  sls::Pairing pairing(kSelf);
  TEST_ASSERT_EQUAL_HEX32(0x772375FC, pairing.group_id());

  pairing.on_long_press(1000);
  pairing.on_hello(kLowPeer, 1100);
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
  TEST_ASSERT_FALSE(pairing.on_long_press(1000).group_formed);

  TEST_ASSERT_FALSE(pairing.on_tick(30999).group_formed);
  TEST_ASSERT_TRUE(pairing.on_tick(31000).group_formed);
  TEST_ASSERT_FALSE(pairing.on_tick(31100).group_formed);
  TEST_ASSERT_FALSE(pairing.on_tick(60000).group_formed);
}

void test_ninth_hello_is_refused_without_evicting_anyone() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000);
  for (uint8_t i = 0; i < 7; ++i) {
    pairing.on_hello({0x02, 0, 0, 0, 0, uint8_t(0x10 + i)}, 1100);
  }
  TEST_ASSERT_EQUAL_UINT(8, pairing.members().size());

  sls::PairingActions actions = pairing.on_hello(kLowPeer, 1200);

  TEST_ASSERT_EQUAL_UINT(8, pairing.members().size());
  TEST_ASSERT_TRUE(kSelf == pairing.members().front());
  TEST_ASSERT_FALSE(actions.send_ack_to.has_value());
}

void test_ninth_ack_is_ignored() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000);
  for (uint8_t i = 0; i < 7; ++i) {
    pairing.on_hello({0x02, 0, 0, 0, 0, uint8_t(0x10 + i)}, 1100);
  }

  pairing.on_ack(kLowPeer, 1200);

  TEST_ASSERT_EQUAL_UINT(8, pairing.members().size());
  TEST_ASSERT_TRUE(kSelf == pairing.members().front());
}

void test_hello_and_ack_after_the_window_closes_are_ignored() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000);
  pairing.on_tick(31000);

  sls::PairingActions hello = pairing.on_hello(kPeer, 32000);
  pairing.on_ack(kLowPeer, 32100);

  TEST_ASSERT_FALSE(hello.send_ack_to.has_value());
  TEST_ASSERT_FALSE(pairing.window_open());
  TEST_ASSERT_EQUAL_UINT(1, pairing.members().size());
}

void test_hello_before_any_press_is_ignored() {
  sls::Pairing pairing(kSelf);

  sls::PairingActions hello = pairing.on_hello(kPeer, 500);

  TEST_ASSERT_FALSE(hello.send_ack_to.has_value());
  TEST_ASSERT_FALSE(pairing.window_open());
  TEST_ASSERT_EQUAL_UINT(1, pairing.members().size());
}

void test_unit_with_no_peers_closes_as_a_group_of_one() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000);

  sls::PairingActions closing = pairing.on_tick(31000);

  TEST_ASSERT_TRUE(closing.group_formed);
  TEST_ASSERT_EQUAL_UINT(1, pairing.members().size());
  TEST_ASSERT_TRUE(kSelf == pairing.members().front());
  TEST_ASSERT_EQUAL_HEX32(0x772375FC, pairing.group_id());
}

void test_new_long_press_starts_pairing_fresh_as_a_group_of_one() {
  sls::Pairing pairing(kSelf);
  pairing.on_long_press(1000);
  pairing.on_hello(kPeer, 1100);
  pairing.on_tick(31000);
  TEST_ASSERT_EQUAL_UINT(2, pairing.members().size());

  pairing.on_long_press(40000);

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

  sls::Pairing restored(self, persisted);

  TEST_ASSERT_FALSE(restored.window_open());
  TEST_ASSERT_TRUE(radio.units[0].members() == restored.members());
  TEST_ASSERT_EQUAL_HEX32(radio.units[0].group_id(), restored.group_id());
}

void test_restored_group_is_deduplicated_and_capped_at_eight() {
  std::vector<sls::Mac> persisted = {kSelf, kPeer, kPeer};
  for (uint8_t i = 0; i < 10; ++i) {
    persisted.push_back({0x02, 0, 0, 0, 0, uint8_t(0x10 + i)});
  }

  sls::Pairing restored(kSelf, persisted);

  TEST_ASSERT_EQUAL_UINT(8, restored.members().size());
  TEST_ASSERT_TRUE(kSelf == restored.members().front());
  TEST_ASSERT_TRUE(restored.members() == [&] { auto m = restored.members(); std::sort(m.begin(), m.end()); return m; }());
  TEST_ASSERT_FALSE(restored.window_open());
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
  RUN_TEST(test_restored_group_is_deduplicated_and_capped_at_eight);
  return UNITY_END();
}
