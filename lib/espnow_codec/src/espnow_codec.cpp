#include "espnow_codec.h"

namespace sls {

namespace {

constexpr uint8_t kMagic[2] = {'S', 'L'};
constexpr size_t kVersionOffset = 2;
constexpr size_t kTypeOffset = 3;
constexpr size_t kGroupOffset = 4;
constexpr size_t kSenderOffset = 8;
constexpr size_t kHeaderBytes = kSenderOffset + 6;
constexpr size_t kPresenceBodyBytes = 1 + 4;
constexpr size_t kChannelMovedBodyBytes = 1;
constexpr size_t kStateBodyBytes = 1 + 2 + 1 + 2 + 1 + 1 + 1 + 1 + 1 + 4 + 4 + 1;

constexpr int kUnknownType = -1;

// Body size in bytes for a message type, or kUnknownType when the type is unknown.
int body_bytes(uint8_t type) {
  switch (static_cast<MessageType>(type)) {
    case MessageType::State:
    case MessageType::Heartbeat:
      return kStateBodyBytes;
    case MessageType::Presence:
      return kPresenceBodyBytes;
    case MessageType::ChannelMoved:
      return kChannelMovedBodyBytes;
    case MessageType::PairingHello:
    case MessageType::PairingAck:
    case MessageType::OverrideClaim:
    case MessageType::MatterSilent:
      return 0;
  }
  return kUnknownType;
}

void put16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v);
  p[1] = static_cast<uint8_t>(v >> 8);
}

void put32(uint8_t* p, uint32_t v) {
  for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}

uint16_t get16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t get32(const uint8_t* p) {
  uint32_t v = 0;
  for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(p[i]) << (8 * i);
  return v;
}

void put_state_body(uint8_t* b, const StateSync& sync) {
  const Look& look = sync.state.look;
  b[0] = static_cast<uint8_t>(look.color_mode);
  put16(b + 1, look.hue_deg);
  b[3] = look.saturation;
  put16(b + 4, look.color_temp_mireds);
  b[6] = look.brightness_pct;
  b[7] = look.blink_on ? 1 : 0;
  b[8] = look.blink_speed_pct;
  b[9] = sync.state.open ? 1 : 0;
  b[10] = sync.state.motion_enabled ? 1 : 0;
  put32(b + 11, sync.seq);
  put32(b + 15, sync.blink_phase_ms);
  b[19] = sync.wifi_channel;
}

// Body bytes of a State/Heartbeat message hold values the receiver can safely apply:
// the same ranges the Group state reducer keeps a Look within.
bool look_is_valid(const uint8_t* b) {
  if (b[0] > static_cast<uint8_t>(ColorMode::ColorTemp)) return false;
  if (get16(b + 1) >= kHueDegrees) return false;
  if (b[6] < kMinOpenLevelPct || b[6] > kMaxLevelPct) return false;
  if (b[8] > kMaxLevelPct) return false;
  return true;
}

StateSync get_state_body(const uint8_t* b) {
  StateSync sync{};
  sync.state.look.color_mode = static_cast<ColorMode>(b[0]);
  sync.state.look.hue_deg = get16(b + 1);
  sync.state.look.saturation = b[3];
  sync.state.look.color_temp_mireds = get16(b + 4);
  sync.state.look.brightness_pct = b[6];
  sync.state.look.blink_on = b[7] != 0;
  sync.state.look.blink_speed_pct = b[8];
  sync.state.open = b[9] != 0;
  sync.state.motion_enabled = b[10] != 0;
  sync.seq = get32(b + 11);
  sync.blink_phase_ms = get32(b + 15);
  sync.wifi_channel = b[19];
  return sync;
}

}  // namespace

size_t encode(const Message& message, uint8_t* out, size_t capacity) {
  const int known_size = body_bytes(static_cast<uint8_t>(message.type));
  if (known_size == kUnknownType) return 0;
  const size_t total = kHeaderBytes + static_cast<size_t>(known_size);

  // The body variant must match the type; check it before writing anything.
  const StateSync* sync = std::get_if<StateSync>(&message.body);
  const Presence* presence = std::get_if<Presence>(&message.body);
  const ChannelMoved* moved = std::get_if<ChannelMoved>(&message.body);
  switch (message.type) {
    case MessageType::State:
    case MessageType::Heartbeat:
      if (sync == nullptr) return 0;
      break;
    case MessageType::Presence:
      if (presence == nullptr) return 0;
      break;
    case MessageType::ChannelMoved:
      if (moved == nullptr) return 0;
      break;
    case MessageType::PairingHello:
    case MessageType::PairingAck:
    case MessageType::OverrideClaim:
    case MessageType::MatterSilent:
      if (!std::holds_alternative<std::monostate>(message.body)) return 0;
      break;
  }
  if (capacity < total) return 0;

  out[0] = kMagic[0];
  out[1] = kMagic[1];
  out[kVersionOffset] = kProtocolVersion;
  out[kTypeOffset] = static_cast<uint8_t>(message.type);
  put32(out + kGroupOffset, message.group_id);
  for (size_t i = 0; i < message.sender.size(); ++i) out[kSenderOffset + i] = message.sender[i];

  uint8_t* b = out + kHeaderBytes;
  switch (message.type) {
    case MessageType::State:
    case MessageType::Heartbeat:
      put_state_body(b, *sync);
      break;
    case MessageType::Presence:
      b[0] = presence->present ? 1 : 0;
      put32(b + 1, presence->total_presence_ms);
      break;
    case MessageType::ChannelMoved:
      b[0] = moved->channel;
      break;
    case MessageType::PairingHello:
    case MessageType::PairingAck:
    case MessageType::OverrideClaim:
    case MessageType::MatterSilent:
      break;
  }
  return total;
}

Decoded decode(const uint8_t* buf, size_t len, uint32_t my_group_id) {
  Decoded result{};
  if (len < kHeaderBytes) {
    result.error = DecodeError::Truncated;
    return result;
  }
  if (buf[0] != kMagic[0] || buf[1] != kMagic[1]) {
    result.error = DecodeError::BadMagic;
    return result;
  }
  if (buf[kVersionOffset] != kProtocolVersion) {
    result.error = DecodeError::BadVersion;
    return result;
  }
  const int known_size = body_bytes(buf[kTypeOffset]);
  if (known_size == kUnknownType) {
    result.error = DecodeError::UnknownType;
    return result;
  }
  const size_t body_size = static_cast<size_t>(known_size);
  const MessageType type = static_cast<MessageType>(buf[kTypeOffset]);
  const uint32_t group_id = get32(buf + kGroupOffset);
  // An unpaired Unit does not know its group yet, so pairing messages skip the group check.
  const bool is_pairing = type == MessageType::PairingHello || type == MessageType::PairingAck;
  if (!is_pairing && group_id != my_group_id) {
    result.error = DecodeError::WrongGroup;
    return result;
  }
  if (len < kHeaderBytes + body_size) {
    result.error = DecodeError::Truncated;
    return result;
  }
  if (len > kHeaderBytes + body_size) {
    result.error = DecodeError::BadLength;
    return result;
  }
  result.message.type = type;
  result.message.group_id = group_id;
  for (size_t i = 0; i < result.message.sender.size(); ++i) result.message.sender[i] = buf[kSenderOffset + i];

  const uint8_t* b = buf + kHeaderBytes;
  switch (type) {
    case MessageType::State:
    case MessageType::Heartbeat:
      if (!look_is_valid(b)) {
        result.error = DecodeError::BadValue;
        return result;
      }
      result.message.body = get_state_body(b);
      break;
    case MessageType::Presence:
      result.message.body = Presence{b[0] != 0, get32(b + 1)};
      break;
    case MessageType::ChannelMoved:
      result.message.body = ChannelMoved{b[0]};
      break;
    case MessageType::PairingHello:
    case MessageType::PairingAck:
    case MessageType::OverrideClaim:
    case MessageType::MatterSilent:
      result.message.body = std::monostate{};
      break;
  }
  result.ok = true;
  return result;
}

bool seq_is_newer(uint32_t candidate, uint32_t current) {
  const uint32_t forward = candidate - current;
  return forward != 0 && forward < 0x80000000u;
}

}  // namespace sls
