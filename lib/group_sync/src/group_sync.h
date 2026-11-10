#pragma once
#include <cstdint>
#include <vector>

#include "blink.h"
#include "espnow_codec.h"
#include "group_state.h"

namespace sls {

// Undecided: not commissioned yet, Matter-capable. Coordinator: the one Member
// commissioned into Matter; owns the Group state. Member: Matter-silent, follows
// the Coordinator. Persisted; it only changes again when the Unit is erased.
enum class SyncRole : uint8_t { Undecided, Coordinator, Member };

// A message to unicast to one Member.
struct Outgoing {
  Mac to;
  Message message;
};

constexpr uint32_t kHeartbeatIntervalMs = 2000;
constexpr uint32_t kBlinkRealignMs = 20;

// Keeps every Member of a Group showing the Coordinator's Group state and
// blinking in step. Returns the messages to send instead of sending them.
class GroupSync {
 public:
  // members: the Group table, sorted, self included.
  GroupSync(const Mac& self, uint32_t group_id, std::vector<Mac> members, SyncRole persisted, const GroupState& initial);

  // This Unit was commissioned into Matter: it becomes Coordinator and tells
  // every other Member to stay Matter-silent.
  std::vector<Outgoing> on_commissioned(uint32_t now_ms);
  // The Group state changed here (voice, motion). A Coordinator relays it as a
  // State to every other Member; an Undecided Unit only keeps it; a Member
  // ignores it. An unchanged state sends nothing, so it can be called every loop.
  std::vector<Outgoing> on_local_change(const GroupState& state, uint32_t now_ms);
  // A Coordinator sends a Heartbeat every kHeartbeatIntervalMs: the Group state,
  // its blink phase and the Wi-Fi channel it is on.
  std::vector<Outgoing> on_tick(uint32_t now_ms, uint8_t wifi_channel);
  // State, Heartbeat and MatterSilent from this Group; anything else is ignored.
  void on_message(const Message& message, uint32_t now_ms);

  SyncRole role() const;
  bool matter_enabled() const;  // false once Member
  const GroupState& state() const;
  // Ms into the blink cycle, in step with the Coordinator on a Member.
  uint32_t blink_phase_ms(uint32_t now_ms) const;
  // When a State or Heartbeat last arrived from the Coordinator.
  uint32_t last_heard_ms() const;

 private:
  std::vector<Outgoing> to_other_members(MessageType type, const StateSync* sync) const;
  void realign_blink(const StateSync& sync, uint32_t now_ms);

  Mac self_;
  uint32_t group_id_;
  std::vector<Mac> members_;
  SyncRole role_;
  GroupState state_;
  BlinkClock blink_clock_;
  // Coordinator side.
  uint32_t seq_ = 0;
  bool sent_heartbeat_ = false;
  uint32_t last_heartbeat_ms_ = 0;
  // Member side.
  bool heard_any_ = false;
  uint32_t last_seq_ = 0;
  uint32_t last_heard_ms_ = 0;
};

}  // namespace sls
