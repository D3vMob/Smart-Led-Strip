#include <unity.h>
#include <cstring>
#include <string>
#include <vector>
#include <variant>
#include "sync_auth.h"

void setUp() {}
void tearDown() {}

static std::vector<uint8_t> from_hex(const char* hex) {
  std::vector<uint8_t> out;
  for (size_t i = 0; hex[i] && hex[i + 1]; i += 2) out.push_back(static_cast<uint8_t>(std::stoi(std::string(hex + i, 2), nullptr, 16)));
  return out;
}

static void assert_digest(const char* expected_hex, const sls::Sha256Digest& actual) {
  std::vector<uint8_t> expected = from_hex(expected_hex);
  TEST_ASSERT_EQUAL_UINT(32, expected.size());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected.data(), actual.data(), 32);
}

void test_sha256_of_abc_matches_the_standard_vector() {
  const char* msg = "abc";
  assert_digest("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                sls::sha256(reinterpret_cast<const uint8_t*>(msg), 3));
}

void test_hmac_sha256_matches_rfc_4231_test_case_1() {
  std::vector<uint8_t> key(20, 0x0b);
  const char* data = "Hi There";
  assert_digest("b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7",
                sls::hmac_sha256(key.data(), key.size(), reinterpret_cast<const uint8_t*>(data), std::strlen(data)));
}

void test_hmac_sha256_matches_rfc_4231_test_case_2() {
  const char* key = "Jefe";
  const char* data = "what do ya want for nothing?";
  assert_digest("5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
                sls::hmac_sha256(reinterpret_cast<const uint8_t*>(key), 4, reinterpret_cast<const uint8_t*>(data), std::strlen(data)));
}

void test_hmac_sha256_matches_rfc_4231_test_case_6_with_a_key_longer_than_the_block() {
  std::vector<uint8_t> key(131, 0xaa);
  const char* data = "Test Using Larger Than Block-Size Key - Hash Key First";
  assert_digest("60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54",
                sls::hmac_sha256(key.data(), key.size(), reinterpret_cast<const uint8_t*>(data), std::strlen(data)));
}

static sls::GroupSecret secret_of(uint8_t fill) {
  sls::GroupSecret secret;
  secret.fill(fill);
  return secret;
}

static sls::Message heartbeat() {
  sls::StateSync sync{};
  sync.state.look.color_mode = sls::ColorMode::HueSat;
  sync.state.look.hue_deg = 200;
  sync.state.look.saturation = 80;
  sync.state.look.brightness_pct = 40;
  sync.state.open = true;
  sync.seq = 77;
  sync.blink_phase_ms = 1234;
  sync.wifi_channel = 6;

  sls::Message message;
  message.type = sls::MessageType::Heartbeat;
  message.group_id = 0xCAFEF00D;
  message.sender = {1, 2, 3, 4, 5, 6};
  message.body = sync;
  message.auth.boot_count = 0x11223344;
  message.auth.counter = 9;
  return message;
}

void test_a_signed_heartbeat_verifies_with_the_group_secret() {
  sls::Message message = heartbeat();
  sls::sign(message, secret_of(0x5a));
  TEST_ASSERT_TRUE(sls::verify(message, secret_of(0x5a)));
}

void test_a_signed_state_and_matter_silent_verify_with_the_group_secret() {
  sls::Message state = heartbeat();
  state.type = sls::MessageType::State;
  sls::sign(state, secret_of(0x5a));
  sls::Message silent = heartbeat();
  silent.type = sls::MessageType::MatterSilent;
  silent.body = std::monostate{};
  sls::sign(silent, secret_of(0x5a));
  TEST_ASSERT_TRUE(sls::verify(state, secret_of(0x5a)) && sls::verify(silent, secret_of(0x5a)));
}

void test_a_heartbeat_altered_after_signing_fails_verification() {
  const sls::GroupSecret secret = secret_of(0x5a);
  sls::Message signed_message = heartbeat();
  sls::sign(signed_message, secret);

  int accepted = 0;
  auto check = [&](auto alter) {
    sls::Message m = signed_message;
    alter(m);
    if (sls::verify(m, secret)) ++accepted;
  };
  check([](sls::Message& m) { std::get<sls::StateSync>(m.body).state.look.brightness_pct = 41; });
  check([](sls::Message& m) { std::get<sls::StateSync>(m.body).wifi_channel = 7; });
  check([](sls::Message& m) { std::get<sls::StateSync>(m.body).seq = 78; });
  check([](sls::Message& m) { m.sender[5] = 7; });
  check([](sls::Message& m) { m.group_id = 0xCAFEF00E; });
  check([](sls::Message& m) { m.type = sls::MessageType::State; });
  check([](sls::Message& m) { m.auth.boot_count += 1; });
  check([](sls::Message& m) { m.auth.counter += 1; });
  TEST_ASSERT_EQUAL_INT(0, accepted);
}

void test_a_heartbeat_signed_with_another_secret_fails() {
  sls::Message message = heartbeat();
  sls::sign(message, secret_of(0x5a));
  TEST_ASSERT_FALSE(sls::verify(message, secret_of(0x5b)));
}

void test_an_unsigned_sync_message_fails_verification() {
  sls::Message message = heartbeat();
  TEST_ASSERT_FALSE(sls::verify(message, secret_of(0x5a)));
}

void test_a_pairing_hello_never_verifies() {
  sls::Message message;
  message.type = sls::MessageType::PairingHello;
  message.group_id = 1;
  message.sender = {1, 2, 3, 4, 5, 6};
  message.body = std::monostate{};
  sls::sign(message, secret_of(0x5a));
  TEST_ASSERT_FALSE(sls::verify(message, secret_of(0x5a)));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_sha256_of_abc_matches_the_standard_vector);
  RUN_TEST(test_hmac_sha256_matches_rfc_4231_test_case_1);
  RUN_TEST(test_hmac_sha256_matches_rfc_4231_test_case_2);
  RUN_TEST(test_hmac_sha256_matches_rfc_4231_test_case_6_with_a_key_longer_than_the_block);
  RUN_TEST(test_a_signed_heartbeat_verifies_with_the_group_secret);
  RUN_TEST(test_a_signed_state_and_matter_silent_verify_with_the_group_secret);
  RUN_TEST(test_a_heartbeat_altered_after_signing_fails_verification);
  RUN_TEST(test_a_heartbeat_signed_with_another_secret_fails);
  RUN_TEST(test_an_unsigned_sync_message_fails_verification);
  RUN_TEST(test_a_pairing_hello_never_verifies);
  return UNITY_END();
}
