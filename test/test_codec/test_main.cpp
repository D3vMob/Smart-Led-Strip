#include <unity.h>
#include <variant>
#include "espnow_codec.h"

void setUp() {}
void tearDown() {}

void test_state_message_round_trips_with_every_field_intact() {
  sls::StateSync sync;
  sync.state.look.color_mode = sls::ColorMode::ColorTemp;
  sync.state.look.hue_deg = 321;
  sync.state.look.saturation = 77;
  sync.state.look.color_temp_mireds = 370;
  sync.state.look.brightness_pct = 42;
  sync.state.look.blink_on = true;
  sync.state.look.blink_speed_pct = 63;
  sync.state.open = true;
  sync.state.motion_enabled = true;
  sync.seq = 0x01020304;
  sync.blink_phase_ms = 0xA1B2C3D4;
  sync.wifi_channel = 11;

  sls::Message message;
  message.type = sls::MessageType::State;
  message.group_id = 0xDEADBEEF;
  message.sender = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
  message.body = sync;

  uint8_t buf[sls::kMaxMessageBytes];
  size_t n = sls::encode(message, buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, n);

  sls::Decoded decoded = sls::decode(buf, n, 0xDEADBEEF);
  TEST_ASSERT_TRUE(decoded.ok);
  TEST_ASSERT_TRUE(decoded.message.type == sls::MessageType::State);
  TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, decoded.message.group_id);
  TEST_ASSERT_TRUE(message.sender == decoded.message.sender);

  const sls::StateSync* got = std::get_if<sls::StateSync>(&decoded.message.body);
  TEST_ASSERT_NOT_NULL(got);
  TEST_ASSERT_TRUE(got->state.look.color_mode == sls::ColorMode::ColorTemp);
  TEST_ASSERT_EQUAL_UINT16(321, got->state.look.hue_deg);
  TEST_ASSERT_EQUAL_UINT8(77, got->state.look.saturation);
  TEST_ASSERT_EQUAL_UINT16(370, got->state.look.color_temp_mireds);
  TEST_ASSERT_EQUAL_UINT8(42, got->state.look.brightness_pct);
  TEST_ASSERT_TRUE(got->state.look.blink_on);
  TEST_ASSERT_EQUAL_UINT8(63, got->state.look.blink_speed_pct);
  TEST_ASSERT_TRUE(got->state.open);
  TEST_ASSERT_TRUE(got->state.motion_enabled);
  TEST_ASSERT_EQUAL_UINT32(0x01020304, got->seq);
  TEST_ASSERT_EQUAL_UINT32(0xA1B2C3D4, got->blink_phase_ms);
  TEST_ASSERT_EQUAL_UINT8(11, got->wifi_channel);
}

void test_heartbeat_message_round_trips_with_every_field_intact() {
  sls::StateSync sync;
  sync.state.look.color_mode = sls::ColorMode::HueSat;
  sync.state.look.hue_deg = 123;
  sync.state.look.saturation = 201;
  sync.state.look.color_temp_mireds = 255;
  sync.state.look.brightness_pct = 88;
  sync.state.look.blink_on = false;
  sync.state.look.blink_speed_pct = 19;
  sync.state.open = false;
  sync.state.motion_enabled = true;
  sync.seq = 0x0A0B0C0D;
  sync.blink_phase_ms = 0x11223344;
  sync.wifi_channel = 6;

  sls::Message message;
  message.type = sls::MessageType::Heartbeat;
  message.group_id = 0x01020304;
  message.sender = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6};
  message.body = sync;

  uint8_t buf[sls::kMaxMessageBytes];
  size_t n = sls::encode(message, buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, n);

  sls::Decoded decoded = sls::decode(buf, n, 0x01020304);
  TEST_ASSERT_TRUE(decoded.ok);
  TEST_ASSERT_TRUE(decoded.message.type == sls::MessageType::Heartbeat);
  TEST_ASSERT_EQUAL_UINT32(0x01020304, decoded.message.group_id);
  TEST_ASSERT_TRUE(message.sender == decoded.message.sender);

  const sls::StateSync* got = std::get_if<sls::StateSync>(&decoded.message.body);
  TEST_ASSERT_NOT_NULL(got);
  TEST_ASSERT_TRUE(got->state.look.color_mode == sls::ColorMode::HueSat);
  TEST_ASSERT_EQUAL_UINT16(123, got->state.look.hue_deg);
  TEST_ASSERT_EQUAL_UINT8(201, got->state.look.saturation);
  TEST_ASSERT_EQUAL_UINT16(255, got->state.look.color_temp_mireds);
  TEST_ASSERT_EQUAL_UINT8(88, got->state.look.brightness_pct);
  TEST_ASSERT_FALSE(got->state.look.blink_on);
  TEST_ASSERT_EQUAL_UINT8(19, got->state.look.blink_speed_pct);
  TEST_ASSERT_FALSE(got->state.open);
  TEST_ASSERT_TRUE(got->state.motion_enabled);
  TEST_ASSERT_EQUAL_UINT32(0x0A0B0C0D, got->seq);
  TEST_ASSERT_EQUAL_UINT32(0x11223344, got->blink_phase_ms);
  TEST_ASSERT_EQUAL_UINT8(6, got->wifi_channel);
}

static void check_presence_round_trip(bool present, uint32_t total_ms) {
  sls::Message message;
  message.type = sls::MessageType::Presence;
  message.group_id = 0x0BADF00D;
  message.sender = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
  message.body = sls::Presence{present, total_ms};

  uint8_t buf[sls::kMaxMessageBytes];
  size_t n = sls::encode(message, buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, n);

  sls::Decoded decoded = sls::decode(buf, n, 0x0BADF00D);
  TEST_ASSERT_TRUE(decoded.ok);
  TEST_ASSERT_TRUE(decoded.message.type == sls::MessageType::Presence);
  TEST_ASSERT_EQUAL_UINT32(0x0BADF00D, decoded.message.group_id);
  TEST_ASSERT_TRUE(message.sender == decoded.message.sender);

  const sls::Presence* got = std::get_if<sls::Presence>(&decoded.message.body);
  TEST_ASSERT_NOT_NULL(got);
  TEST_ASSERT_EQUAL(present, got->present);
  TEST_ASSERT_EQUAL_UINT32(total_ms, got->total_presence_ms);
}

void test_presence_message_round_trips_onset_and_absence() {
  check_presence_round_trip(true, 0x01020304);
  check_presence_round_trip(false, 0xF1E2D3C4);
}

static void check_pairing_decodes_for_unpaired_unit(sls::MessageType type) {
  sls::Message message;
  message.type = type;
  message.group_id = 0xA1B2C3D4;
  message.sender = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
  message.body = sls::PairingInfo{};

  uint8_t buf[sls::kMaxMessageBytes];
  size_t n = sls::encode(message, buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, n);

  sls::Decoded decoded = sls::decode(buf, n, 0);
  TEST_ASSERT_TRUE(decoded.ok);
  TEST_ASSERT_TRUE(decoded.message.type == type);
  TEST_ASSERT_EQUAL_UINT32(0xA1B2C3D4, decoded.message.group_id);
  TEST_ASSERT_TRUE(message.sender == decoded.message.sender);
}

void test_pairing_hello_and_ack_decode_for_unpaired_unit() {
  check_pairing_decodes_for_unpaired_unit(sls::MessageType::PairingHello);
  check_pairing_decodes_for_unpaired_unit(sls::MessageType::PairingAck);
}

void test_state_message_for_another_group_is_rejected_as_wrong_group() {
  sls::StateSync sync{};
  sls::Message message;
  message.type = sls::MessageType::State;
  message.group_id = 7;
  message.sender = {1, 2, 3, 4, 5, 6};
  message.body = sync;

  uint8_t buf[sls::kMaxMessageBytes];
  size_t n = sls::encode(message, buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, n);

  sls::Decoded decoded = sls::decode(buf, n, 8);
  TEST_ASSERT_FALSE(decoded.ok);
  TEST_ASSERT_TRUE(decoded.error == sls::DecodeError::WrongGroup);
}

void test_override_claim_channel_moved_and_matter_silent_round_trip() {
  const sls::MessageType types[] = {sls::MessageType::OverrideClaim,
                                    sls::MessageType::MatterSilent,
                                    sls::MessageType::ChannelMoved};
  for (sls::MessageType type : types) {
    sls::Message message;
    message.type = type;
    message.group_id = 0x5566AABB;
    message.sender = {0x71, 0x72, 0x73, 0x74, 0x75, 0x76};
    if (type == sls::MessageType::ChannelMoved) {
      message.body = sls::ChannelMoved{11};
    } else {
      message.body = std::monostate{};
    }

    uint8_t buf[sls::kMaxMessageBytes];
    size_t n = sls::encode(message, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, n);

    sls::Decoded decoded = sls::decode(buf, n, 0x5566AABB);
    TEST_ASSERT_TRUE(decoded.ok);
    TEST_ASSERT_TRUE(decoded.message.type == type);
    TEST_ASSERT_EQUAL_UINT32(0x5566AABB, decoded.message.group_id);
    TEST_ASSERT_TRUE(message.sender == decoded.message.sender);
    if (type == sls::MessageType::ChannelMoved) {
      const sls::ChannelMoved* got = std::get_if<sls::ChannelMoved>(&decoded.message.body);
      TEST_ASSERT_NOT_NULL(got);
      TEST_ASSERT_EQUAL_UINT8(11, got->channel);
    } else {
      TEST_ASSERT_TRUE(std::holds_alternative<std::monostate>(decoded.message.body));
    }
  }
}

void test_override_claim_for_another_group_is_rejected_as_wrong_group() {
  sls::Message message;
  message.type = sls::MessageType::OverrideClaim;
  message.group_id = 100;
  message.sender = {1, 2, 3, 4, 5, 6};
  message.body = std::monostate{};

  uint8_t buf[sls::kMaxMessageBytes];
  size_t n = sls::encode(message, buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, n);

  sls::Decoded decoded = sls::decode(buf, n, 101);
  TEST_ASSERT_FALSE(decoded.ok);
  TEST_ASSERT_TRUE(decoded.error == sls::DecodeError::WrongGroup);
}

static const sls::MessageType kAllTypes[] = {
    sls::MessageType::State,         sls::MessageType::Heartbeat,
    sls::MessageType::Presence,      sls::MessageType::PairingHello,
    sls::MessageType::PairingAck,    sls::MessageType::OverrideClaim,
    sls::MessageType::ChannelMoved,  sls::MessageType::MatterSilent};

static sls::SyncAuth sample_auth() {
  sls::SyncAuth auth;
  auth.boot_count = 0xCAFEF00D;
  auth.counter = 0x00000102;
  for (size_t i = 0; i < auth.tag.size(); ++i) auth.tag[i] = static_cast<uint8_t>(0xB0 + i);
  return auth;
}

static sls::Message sample_message(sls::MessageType type) {
  sls::Message message;
  message.type = type;
  message.group_id = 0x12345678;
  message.sender = {9, 8, 7, 6, 5, 4};
  switch (type) {
    case sls::MessageType::State:
    case sls::MessageType::Heartbeat: {
      sls::StateSync sync{};
      sync.state.look.brightness_pct = 50;  // a Look a receiver would accept
      sync.state.look.blink_speed_pct = 20;
      message.body = sync;
      break;
    }
    case sls::MessageType::Presence:
      message.body = sls::Presence{true, 1000};
      break;
    case sls::MessageType::ChannelMoved:
      message.body = sls::ChannelMoved{6};
      break;
    case sls::MessageType::PairingHello:
    case sls::MessageType::PairingAck:
      message.body = sls::PairingInfo{1234, {{1, 2, 3, 4, 5, 6}, {7, 8, 9, 10, 11, 12}}};
      break;
    default:
      message.body = std::monostate{};
      break;
  }
  return message;
}

void test_every_truncated_buffer_is_rejected_as_truncated() {
  for (sls::MessageType type : kAllTypes) {
    uint8_t buf[sls::kMaxMessageBytes];
    size_t full = sls::encode(sample_message(type), buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, full);
    for (size_t len = 0; len < full; ++len) {
      sls::Decoded decoded = sls::decode(buf, len, 0x12345678);
      TEST_ASSERT_FALSE(decoded.ok);
      TEST_ASSERT_TRUE(decoded.error == sls::DecodeError::Truncated);
    }
  }
}

void test_buffer_longer_than_the_type_layout_is_rejected_as_bad_length() {
  for (sls::MessageType type : kAllTypes) {
    uint8_t buf[sls::kMaxMessageBytes] = {};
    size_t full = sls::encode(sample_message(type), buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, full);
    sls::Decoded decoded = sls::decode(buf, full + 1, 0x12345678);
    TEST_ASSERT_FALSE(decoded.ok);
    TEST_ASSERT_TRUE(decoded.error == sls::DecodeError::BadLength);
  }
}

static size_t encode_sample_state(uint8_t* buf) {
  size_t n = sls::encode(sample_message(sls::MessageType::State), buf, sls::kMaxMessageBytes);
  TEST_ASSERT_GREATER_THAN(0, n);
  return n;
}

static sls::Decoded decode_sample_state_with_byte(size_t index, uint8_t value) {
  uint8_t buf[sls::kMaxMessageBytes];
  size_t n = encode_sample_state(buf);
  buf[index] = value;
  return sls::decode(buf, n, 0x12345678);
}

static sls::DecodeError decode_error_after_corrupting(size_t index, uint8_t value) {
  sls::Decoded decoded = decode_sample_state_with_byte(index, value);
  TEST_ASSERT_FALSE(decoded.ok);
  return decoded.error;
}

void test_wrong_magic_byte_is_rejected_as_bad_magic() {
  TEST_ASSERT_TRUE(decode_error_after_corrupting(0, 'X') == sls::DecodeError::BadMagic);
  TEST_ASSERT_TRUE(decode_error_after_corrupting(1, 'X') == sls::DecodeError::BadMagic);
}

void test_other_protocol_version_is_rejected_as_bad_version() {
  TEST_ASSERT_TRUE(decode_error_after_corrupting(2, sls::kProtocolVersion + 1) ==
                   sls::DecodeError::BadVersion);
}

void test_type_byte_outside_known_types_is_rejected_as_unknown_type() {
  TEST_ASSERT_TRUE(decode_error_after_corrupting(3, 0) == sls::DecodeError::UnknownType);
  TEST_ASSERT_TRUE(decode_error_after_corrupting(
                       3, static_cast<uint8_t>(sls::MessageType::MatterSilent) + 1) ==
                   sls::DecodeError::UnknownType);
}

void test_color_mode_byte_that_is_not_a_known_mode_is_rejected_as_bad_value() {
  // The color_mode byte is the first byte of the State body, right after the 14-byte header.
  TEST_ASSERT_TRUE(decode_error_after_corrupting(14, 7) == sls::DecodeError::BadValue);
}

// Wire offsets of the State/Heartbeat body fields (header is 14 bytes).
static const size_t kHueOffset = 15;  // uint16 little-endian
static const size_t kBrightnessOffset = 20;
static const size_t kBlinkSpeedOffset = 22;

static sls::Decoded decode_sample_state_with_hue(uint16_t hue_deg) {
  uint8_t buf[sls::kMaxMessageBytes];
  size_t n = encode_sample_state(buf);
  buf[kHueOffset] = static_cast<uint8_t>(hue_deg & 0xFF);
  buf[kHueOffset + 1] = static_cast<uint8_t>(hue_deg >> 8);
  return sls::decode(buf, n, 0x12345678);
}

void test_hue_of_360_or_more_is_rejected_as_bad_value() {
  sls::Decoded decoded = decode_sample_state_with_hue(360);
  TEST_ASSERT_FALSE(decoded.ok);
  TEST_ASSERT_TRUE(decoded.error == sls::DecodeError::BadValue);
}

void test_hue_of_359_is_accepted() {
  TEST_ASSERT_TRUE(decode_sample_state_with_hue(359).ok);
}

void test_brightness_outside_10_to_100_percent_is_rejected_as_bad_value() {
  sls::Decoded too_dim = decode_sample_state_with_byte(kBrightnessOffset, 9);
  TEST_ASSERT_FALSE(too_dim.ok);
  TEST_ASSERT_TRUE(too_dim.error == sls::DecodeError::BadValue);
  sls::Decoded too_bright = decode_sample_state_with_byte(kBrightnessOffset, 101);
  TEST_ASSERT_FALSE(too_bright.ok);
  TEST_ASSERT_TRUE(too_bright.error == sls::DecodeError::BadValue);
}

void test_brightness_of_10_and_100_percent_is_accepted() {
  TEST_ASSERT_TRUE(decode_sample_state_with_byte(kBrightnessOffset, 10).ok);
  TEST_ASSERT_TRUE(decode_sample_state_with_byte(kBrightnessOffset, 100).ok);
}

void test_blink_speed_above_100_percent_is_rejected_as_bad_value() {
  sls::Decoded decoded = decode_sample_state_with_byte(kBlinkSpeedOffset, 101);
  TEST_ASSERT_FALSE(decoded.ok);
  TEST_ASSERT_TRUE(decoded.error == sls::DecodeError::BadValue);
}

void test_blink_speed_of_100_percent_is_accepted() {
  TEST_ASSERT_TRUE(decode_sample_state_with_byte(kBlinkSpeedOffset, 100).ok);
}

void test_blink_speed_of_0_percent_is_rejected_as_bad_value() {
  sls::Decoded decoded = decode_sample_state_with_byte(kBlinkSpeedOffset, 0);
  TEST_ASSERT_FALSE(decoded.ok);
  TEST_ASSERT_TRUE(decoded.error == sls::DecodeError::BadValue);
}

void test_blink_speed_of_1_percent_is_accepted() {
  TEST_ASSERT_TRUE(decode_sample_state_with_byte(kBlinkSpeedOffset, 1).ok);
}

void test_encoded_length_is_within_the_esp_now_limit() {
  for (sls::MessageType type : kAllTypes) {
    uint8_t buf[sls::kMaxMessageBytes];
    size_t n = sls::encode(sample_message(type), buf, sizeof(buf));
    TEST_ASSERT_GREATER_OR_EQUAL(1, n);
    TEST_ASSERT_LESS_OR_EQUAL(sls::kMaxMessageBytes, n);
  }
}

void test_encode_into_too_small_buffer_returns_zero_and_writes_nothing_past_capacity() {
  const uint8_t kSentinel = 0xEE;
  for (sls::MessageType type : kAllTypes) {
    uint8_t probe[sls::kMaxMessageBytes];
    size_t full = sls::encode(sample_message(type), probe, sizeof(probe));
    TEST_ASSERT_GREATER_THAN(0, full);

    uint8_t buf[sls::kMaxMessageBytes + 8];
    for (uint8_t& byte : buf) byte = kSentinel;
    size_t capacity = full - 1;
    TEST_ASSERT_EQUAL_UINT32(0, sls::encode(sample_message(type), buf, capacity));
    for (size_t i = capacity; i < sizeof(buf); ++i) TEST_ASSERT_EQUAL_UINT8(kSentinel, buf[i]);
  }
}

void test_encode_rejects_body_that_does_not_match_the_type() {
  uint8_t buf[sls::kMaxMessageBytes];

  sls::Message state_with_presence_body = sample_message(sls::MessageType::State);
  state_with_presence_body.body = sls::Presence{true, 5};
  TEST_ASSERT_EQUAL_UINT32(0, sls::encode(state_with_presence_body, buf, sizeof(buf)));

  sls::Message presence_with_empty_body = sample_message(sls::MessageType::Presence);
  presence_with_empty_body.body = std::monostate{};
  TEST_ASSERT_EQUAL_UINT32(0, sls::encode(presence_with_empty_body, buf, sizeof(buf)));

  sls::Message claim_with_state_body = sample_message(sls::MessageType::OverrideClaim);
  claim_with_state_body.body = sls::StateSync{};
  TEST_ASSERT_EQUAL_UINT32(0, sls::encode(claim_with_state_body, buf, sizeof(buf)));
}

void test_later_sequence_number_is_newer_and_earlier_or_equal_is_not() {
  TEST_ASSERT_TRUE(sls::seq_is_newer(6, 5));
  TEST_ASSERT_FALSE(sls::seq_is_newer(5, 6));
  TEST_ASSERT_FALSE(sls::seq_is_newer(5, 5));
}

void test_sequence_number_after_wrap_around_is_newer() {
  TEST_ASSERT_TRUE(sls::seq_is_newer(0, 0xFFFFFFFF));
  TEST_ASSERT_TRUE(sls::seq_is_newer(3, 0xFFFFFFFE));
  TEST_ASSERT_FALSE(sls::seq_is_newer(0xFFFFFFFF, 2));
  TEST_ASSERT_TRUE(sls::seq_is_newer(0x7FFFFFFF, 0));
}

static sls::Mac member_mac(uint8_t seed) {
  return {seed, static_cast<uint8_t>(seed + 1), static_cast<uint8_t>(seed + 2),
          static_cast<uint8_t>(seed + 3), static_cast<uint8_t>(seed + 4),
          static_cast<uint8_t>(seed + 5)};
}

static void check_pairing_info_round_trips(sls::MessageType type, uint32_t age_ms, size_t count) {
  sls::PairingInfo info{age_ms, {}};
  for (size_t i = 0; i < count; ++i) info.members.push_back(member_mac(static_cast<uint8_t>(16 * i)));
  sls::Message message = sample_message(type);
  message.body = info;

  uint8_t buf[sls::kMaxMessageBytes];
  size_t n = sls::encode(message, buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, n);

  sls::Decoded decoded = sls::decode(buf, n, 0);
  TEST_ASSERT_TRUE(decoded.ok);
  const sls::PairingInfo* got = std::get_if<sls::PairingInfo>(&decoded.message.body);
  TEST_ASSERT_NOT_NULL(got);
  TEST_ASSERT_EQUAL_UINT32(age_ms, got->window_age_ms);
  TEST_ASSERT_TRUE(info.members == got->members);
}

void test_pairing_hello_and_ack_round_trip_window_age_and_member_table() {
  check_pairing_info_round_trips(sls::MessageType::PairingHello, 0xA1B2C3D4, sls::kMaxGroupMembers);
  check_pairing_info_round_trips(sls::MessageType::PairingAck, 29999, sls::kMaxGroupMembers);
  check_pairing_info_round_trips(sls::MessageType::PairingHello, 0, 0);
  check_pairing_info_round_trips(sls::MessageType::PairingAck, 5, 1);
}

void test_pairing_messages_keep_their_nonce_across_encode_and_decode() {
  sls::PairingInfo info{7, {{1, 2, 3, 4, 5, 6}}};
  for (size_t i = 0; i < info.nonce.size(); ++i) info.nonce[i] = uint8_t(0xA0 + i);
  sls::Message message = sample_message(sls::MessageType::PairingAck);
  message.body = info;
  uint8_t buf[sls::kMaxMessageBytes];
  size_t n = sls::encode(message, buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, n);
  sls::Decoded decoded = sls::decode(buf, n, 0);
  TEST_ASSERT_TRUE(decoded.ok);
  const sls::PairingInfo* got = std::get_if<sls::PairingInfo>(&decoded.message.body);
  TEST_ASSERT_NOT_NULL(got);
  TEST_ASSERT_TRUE(info.nonce == got->nonce);
}

void test_pairing_frame_claiming_more_than_8_members_is_rejected_as_bad_value() {
  uint8_t buf[sls::kMaxMessageBytes] = {};
  size_t n = sls::encode(sample_message(sls::MessageType::PairingHello), buf, sizeof(buf));
  TEST_ASSERT_GREATER_THAN(0, n);
  const size_t kCountOffset = 14 + 4;
  buf[kCountOffset] = 9;
  size_t len = 14 + 5 + sls::kPairingNonceBytes + 6 * 9;
  sls::Decoded decoded = sls::decode(buf, len, 0);
  TEST_ASSERT_FALSE(decoded.ok);
  TEST_ASSERT_TRUE(decoded.error == sls::DecodeError::BadValue);
}

void test_pairing_frame_cut_short_in_the_member_table_is_rejected_as_truncated() {
  uint8_t buf[sls::kMaxMessageBytes] = {};
  size_t n = sls::encode(sample_message(sls::MessageType::PairingAck), buf, sizeof(buf));
  sls::Decoded decoded = sls::decode(buf, n - 1, 0);
  TEST_ASSERT_FALSE(decoded.ok);
  TEST_ASSERT_TRUE(decoded.error == sls::DecodeError::Truncated);
}

void test_pairing_frame_with_trailing_bytes_is_rejected_as_bad_length() {
  uint8_t buf[sls::kMaxMessageBytes] = {};
  size_t n = sls::encode(sample_message(sls::MessageType::PairingAck), buf, sizeof(buf));
  sls::Decoded decoded = sls::decode(buf, n + 6, 0);
  TEST_ASSERT_FALSE(decoded.ok);
  TEST_ASSERT_TRUE(decoded.error == sls::DecodeError::BadLength);
}

void test_encode_rejects_pairing_table_with_more_than_8_members() {
  sls::PairingInfo info{0, {}};
  for (size_t i = 0; i < sls::kMaxGroupMembers + 1; ++i) info.members.push_back(member_mac(1));
  sls::Message message = sample_message(sls::MessageType::PairingHello);
  message.body = info;
  uint8_t buf[sls::kMaxMessageBytes];
  TEST_ASSERT_EQUAL_UINT32(0, sls::encode(message, buf, sizeof(buf)));
}

void test_encode_rejects_empty_body_for_pairing_messages() {
  sls::Message message = sample_message(sls::MessageType::PairingHello);
  message.body = std::monostate{};
  uint8_t buf[sls::kMaxMessageBytes];
  TEST_ASSERT_EQUAL_UINT32(0, sls::encode(message, buf, sizeof(buf)));
}

void test_state_wire_layout_matches_the_golden_bytes() {
  sls::StateSync sync;
  sync.state.look.color_mode = sls::ColorMode::ColorTemp;  // 1
  sync.state.look.hue_deg = 0x0123;
  sync.state.look.saturation = 0x45;
  sync.state.look.color_temp_mireds = 0x0172;
  sync.state.look.brightness_pct = 0x37;
  sync.state.look.blink_on = true;
  sync.state.look.blink_speed_pct = 0x2B;
  sync.state.open = true;
  sync.state.motion_enabled = false;
  sync.seq = 0x01020304;
  sync.blink_phase_ms = 0x0A0B0C0D;
  sync.wifi_channel = 6;

  sls::Message message;
  message.type = sls::MessageType::State;
  message.group_id = 0x11223344;
  message.sender = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5};
  message.body = sync;
  message.auth = sample_auth();

  const uint8_t expected[] = {
      'S', 'L', 3, 1,                          // magic, version, type
      0x44, 0x33, 0x22, 0x11,                  // group id, little-endian
      0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5,      // sender MAC
      0x01,                                    // color_mode
      0x23, 0x01,                              // hue_deg
      0x45,                                    // saturation
      0x72, 0x01,                              // color_temp_mireds
      0x37,                                    // brightness_pct
      0x01,                                    // blink_on
      0x2B,                                    // blink_speed_pct
      0x01,                                    // open
      0x00,                                    // motion_enabled
      0x04, 0x03, 0x02, 0x01,                  // seq
      0x0D, 0x0C, 0x0B, 0x0A,                  // blink_phase_ms
      0x06,                                    // wifi_channel
      0x0D, 0xF0, 0xFE, 0xCA,                  // auth boot_count
      0x02, 0x01, 0x00, 0x00,                  // auth counter
      0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
      0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF};  // auth tag

  uint8_t buf[sls::kMaxMessageBytes];
  size_t n = sls::encode(message, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_UINT32(sizeof(expected), n);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, sizeof(expected));

  sls::Decoded decoded = sls::decode(expected, sizeof(expected), 0x11223344);
  TEST_ASSERT_TRUE(decoded.ok);
  TEST_ASSERT_TRUE(decoded.message.type == sls::MessageType::State);
  TEST_ASSERT_EQUAL_UINT32(0x11223344, decoded.message.group_id);
  TEST_ASSERT_TRUE(message.sender == decoded.message.sender);
  const sls::StateSync* got = std::get_if<sls::StateSync>(&decoded.message.body);
  TEST_ASSERT_NOT_NULL(got);
  TEST_ASSERT_TRUE(got->state.look.color_mode == sls::ColorMode::ColorTemp);
  TEST_ASSERT_EQUAL_UINT16(0x0123, got->state.look.hue_deg);
  TEST_ASSERT_EQUAL_UINT8(0x45, got->state.look.saturation);
  TEST_ASSERT_EQUAL_UINT16(0x0172, got->state.look.color_temp_mireds);
  TEST_ASSERT_EQUAL_UINT8(0x37, got->state.look.brightness_pct);
  TEST_ASSERT_TRUE(got->state.look.blink_on);
  TEST_ASSERT_EQUAL_UINT8(0x2B, got->state.look.blink_speed_pct);
  TEST_ASSERT_TRUE(got->state.open);
  TEST_ASSERT_FALSE(got->state.motion_enabled);
  TEST_ASSERT_EQUAL_UINT32(0x01020304, got->seq);
  TEST_ASSERT_EQUAL_UINT32(0x0A0B0C0D, got->blink_phase_ms);
  TEST_ASSERT_EQUAL_UINT8(6, got->wifi_channel);
}

void test_sync_messages_keep_their_auth_fields_across_encode_and_decode() {
  const sls::MessageType synced[] = {sls::MessageType::State, sls::MessageType::Heartbeat,
                                     sls::MessageType::MatterSilent, sls::MessageType::Presence,
                                     sls::MessageType::OverrideClaim};
  for (sls::MessageType type : synced) {
    sls::Message message = sample_message(type);
    message.auth = sample_auth();
    uint8_t buf[sls::kMaxMessageBytes];
    size_t n = sls::encode(message, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, n);

    sls::Decoded decoded = sls::decode(buf, n, 0x12345678);
    TEST_ASSERT_TRUE(decoded.ok);
    TEST_ASSERT_EQUAL_UINT32(0xCAFEF00D, decoded.message.auth.boot_count);
    TEST_ASSERT_EQUAL_UINT32(0x102, decoded.message.auth.counter);
    TEST_ASSERT_TRUE(message.auth.tag == decoded.message.auth.tag);
  }
}

void test_sync_frame_one_byte_short_of_the_auth_trailer_is_rejected_as_truncated() {
  const sls::MessageType synced[] = {sls::MessageType::State, sls::MessageType::Heartbeat,
                                     sls::MessageType::MatterSilent, sls::MessageType::Presence,
                                     sls::MessageType::OverrideClaim};
  for (sls::MessageType type : synced) {
    uint8_t buf[sls::kMaxMessageBytes];
    size_t full = sls::encode(sample_message(type), buf, sizeof(buf));
    sls::Decoded decoded = sls::decode(buf, full - 1, 0x12345678);
    TEST_ASSERT_FALSE(decoded.ok);
    TEST_ASSERT_TRUE(decoded.error == sls::DecodeError::Truncated);
  }
}

void test_channel_moved_carries_no_auth_bytes() {
  uint8_t buf[sls::kMaxMessageBytes];
  TEST_ASSERT_EQUAL_UINT32(14 + 1, sls::encode(sample_message(sls::MessageType::ChannelMoved), buf, sizeof(buf)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_state_message_round_trips_with_every_field_intact);
  RUN_TEST(test_sync_messages_keep_their_auth_fields_across_encode_and_decode);
  RUN_TEST(test_sync_frame_one_byte_short_of_the_auth_trailer_is_rejected_as_truncated);
  RUN_TEST(test_channel_moved_carries_no_auth_bytes);
  RUN_TEST(test_heartbeat_message_round_trips_with_every_field_intact);
  RUN_TEST(test_presence_message_round_trips_onset_and_absence);
  RUN_TEST(test_pairing_hello_and_ack_decode_for_unpaired_unit);
  RUN_TEST(test_state_message_for_another_group_is_rejected_as_wrong_group);
  RUN_TEST(test_override_claim_channel_moved_and_matter_silent_round_trip);
  RUN_TEST(test_override_claim_for_another_group_is_rejected_as_wrong_group);
  RUN_TEST(test_every_truncated_buffer_is_rejected_as_truncated);
  RUN_TEST(test_buffer_longer_than_the_type_layout_is_rejected_as_bad_length);
  RUN_TEST(test_wrong_magic_byte_is_rejected_as_bad_magic);
  RUN_TEST(test_other_protocol_version_is_rejected_as_bad_version);
  RUN_TEST(test_type_byte_outside_known_types_is_rejected_as_unknown_type);
  RUN_TEST(test_color_mode_byte_that_is_not_a_known_mode_is_rejected_as_bad_value);
  RUN_TEST(test_hue_of_360_or_more_is_rejected_as_bad_value);
  RUN_TEST(test_hue_of_359_is_accepted);
  RUN_TEST(test_brightness_outside_10_to_100_percent_is_rejected_as_bad_value);
  RUN_TEST(test_brightness_of_10_and_100_percent_is_accepted);
  RUN_TEST(test_blink_speed_above_100_percent_is_rejected_as_bad_value);
  RUN_TEST(test_blink_speed_of_100_percent_is_accepted);
  RUN_TEST(test_blink_speed_of_0_percent_is_rejected_as_bad_value);
  RUN_TEST(test_blink_speed_of_1_percent_is_accepted);
  RUN_TEST(test_encoded_length_is_within_the_esp_now_limit);
  RUN_TEST(test_encode_into_too_small_buffer_returns_zero_and_writes_nothing_past_capacity);
  RUN_TEST(test_encode_rejects_body_that_does_not_match_the_type);
  RUN_TEST(test_later_sequence_number_is_newer_and_earlier_or_equal_is_not);
  RUN_TEST(test_sequence_number_after_wrap_around_is_newer);
  RUN_TEST(test_state_wire_layout_matches_the_golden_bytes);
  RUN_TEST(test_pairing_hello_and_ack_round_trip_window_age_and_member_table);
  RUN_TEST(test_pairing_messages_keep_their_nonce_across_encode_and_decode);
  RUN_TEST(test_pairing_frame_claiming_more_than_8_members_is_rejected_as_bad_value);
  RUN_TEST(test_pairing_frame_cut_short_in_the_member_table_is_rejected_as_truncated);
  RUN_TEST(test_pairing_frame_with_trailing_bytes_is_rejected_as_bad_length);
  RUN_TEST(test_encode_rejects_pairing_table_with_more_than_8_members);
  RUN_TEST(test_encode_rejects_empty_body_for_pairing_messages);
  return UNITY_END();
}
