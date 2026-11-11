#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "group_state.h"

namespace sls {

constexpr size_t kMaxMessageBytes = 250;
constexpr uint8_t kProtocolVersion = 4;
constexpr size_t kMaxGroupMembers = 8;
constexpr size_t kSyncTagBytes = 16;

using Mac = std::array<uint8_t, 6>;

enum class MessageType : uint8_t {
  State = 1,
  Heartbeat,
  Presence,
  PairingHello,
  PairingAck,
  OverrideClaim,
  ChannelMoved,
  MatterSilent
};

// Group sync messages: the ones that carry a SyncAuth trailer and are signed with the Group secret.
constexpr bool is_sync_message(MessageType type) {
  return type == MessageType::State || type == MessageType::Heartbeat || type == MessageType::MatterSilent ||
         type == MessageType::Presence || type == MessageType::OverrideClaim ||
         type == MessageType::ChannelMoved;
}

struct StateSync {
  GroupState state;
  uint32_t seq;
  uint32_t blink_phase_ms;
  uint8_t wifi_channel;
  std::optional<Mac> motion_master;  // the Coordinator's current Motion master, or none
};

struct Presence {
  bool present;
  uint32_t total_presence_ms;
  bool open;  // the sender's current Group Open/Closed
  // the sender sent this while it considered the Coordinator silent, or it is its first report after the Coordinator returned
  bool coordinator_silent;
};

struct ChannelMoved {
  uint8_t channel;
};

constexpr size_t kPairingNonceBytes = 16;
using PairingNonce = std::array<uint8_t, kPairingNonceBytes>;

struct PairingInfo {
  uint32_t window_age_ms;  // ms since the sender's first press of this pairing session
  std::vector<Mac> members;  // sender's member table, at most kMaxGroupMembers
  PairingNonce nonce{};      // sender's random contribution to the Group secret
};

// Authentication trailer carried by State, Heartbeat, MatterSilent, Presence, OverrideClaim and ChannelMoved messages.
struct SyncAuth {
  uint32_t boot_count = 0;  // the sender's boot counter, kept in flash and raised on every boot
  uint32_t counter = 0;     // rises with every sync message the sender sends this boot
  std::array<uint8_t, kSyncTagBytes> tag{};  // HMAC over the frame
};

struct Message {
  MessageType type;
  uint32_t group_id;
  Mac sender;
  std::variant<std::monostate, StateSync, Presence, ChannelMoved, PairingInfo> body;
  SyncAuth auth{};  // only encoded for State, Heartbeat, MatterSilent, Presence, OverrideClaim and ChannelMoved
};

size_t encode(const Message& message, uint8_t* out, size_t capacity);

enum class DecodeError : uint8_t {
  None,
  Truncated,
  BadMagic,
  BadVersion,
  UnknownType,
  WrongGroup,
  BadLength,
  BadValue
};

struct Decoded {
  bool ok;
  DecodeError error;
  Message message;
};

Decoded decode(const uint8_t* buf, size_t len, uint32_t my_group_id);

// True when `candidate` was sent after `current`, tolerating 32-bit wrap-around.
bool seq_is_newer(uint32_t candidate, uint32_t current);

}  // namespace sls
