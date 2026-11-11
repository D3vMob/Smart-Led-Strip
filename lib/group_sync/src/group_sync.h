#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "blink.h"
#include "espnow_codec.h"
#include "group_state.h"
#include "sync_auth.h"

namespace sls {

// Undecided: not commissioned yet, Matter-capable. Coordinator: the one Member
// commissioned into Matter; owns the Group state. Member: Matter-silent, follows
// the Coordinator. Saved in flash (see SyncRecord); it only changes again on re-pairing or a flash erase.
enum class SyncRole : uint8_t { Undecided, Coordinator, Member };

// What a Unit saves in flash about its place in the Group: its role and, on a Member, the Coordinator it follows.
struct SyncRecord {
  SyncRole role = SyncRole::Undecided;
  std::optional<Mac> coordinator;
};

// A message to unicast to one Member.
struct Outgoing {
  Mac to;
  Message message;
};

constexpr uint32_t kHeartbeatIntervalMs = 2000;
constexpr uint32_t kBlinkRealignMs = 20;
// A Unit other than the Coordinator that has heard nothing from it for this long starts
// hunting for it across the Wi-Fi channels.
constexpr uint32_t kChannelHuntQuietMs = 6000;
// Time spent listening on each channel while hunting; longer than one Heartbeat interval.
constexpr uint32_t kChannelHuntDwellMs = 2500;
constexpr uint8_t kChannelHuntLast = 13;
// How many of the Coordinator's earlier boot nonces a Member remembers to refuse replays from them.
constexpr size_t kRetiredNonces = 8;

// Keeps every Member of a Group showing the Coordinator's Group state and
// blinking in step. Returns the messages to send instead of sending them.
class GroupSync {
 public:
  // members: the Group table, sorted, self included. secret: the Group secret every
  // sent message is signed with and every received one is verified against.
  // boot_nonce: identifies this boot in the auth block of every sent message.
  GroupSync(const Mac& self, uint32_t group_id, std::vector<Mac> members, const GroupSecret& secret,
            const SyncRecord& persisted, uint32_t boot_nonce, const GroupState& initial);

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
  // Hearing any of them turns an Undecided Unit into a Member; a Coordinator keeps its role.
  // Accepted only when: the message carries a valid tag for the Group secret (a wrong or
  // missing tag is ignored), this Unit has other Group members, from (the radio source MAC)
  // is a Group member other than this Unit and equals message.sender. A Member
  // learns its Coordinator from the first MatterSilent, State or Heartbeat it
  // accepts (unless the persisted record already names it; save record() when it
  // changes) and from then on ignores all of them from anyone else.
  // The message must also be fresh for the Coordinator's boot: within the current boot
  // nonce the counter must be newer than the last accepted one; a nonce never seen
  // means the Coordinator rebooted and becomes the current one; a nonce of an earlier
  // boot (the last kRetiredNonces are remembered) is ignored, so a captured frame
  // cannot be replayed.
  // Ignored messages have no effect at all.
  // Returns true when the message was applied, false when it was ignored.
  bool on_message(const Message& message, const Mac& from, uint32_t now_ms);

  // Matter commissioning is scanning for or joining the router; the radio must
  // not move meanwhile, so listen_channel returns 0 until it stops.
  void on_commissioning(bool in_progress);

  // The Unit was re-paired: a new Group table and Group secret. The role and the
  // Coordinator pin are cleared (record() becomes {Undecided, none}) along with
  // what was heard from the old Coordinator, so the new Group elects its Coordinator again.
  // The Group state and blink clock are kept.
  void regroup(uint32_t group_id, std::vector<Mac> members, const GroupSecret& secret);

  SyncRole role() const;
  // The current role and pinned Coordinator, to save in flash when it changes.
  SyncRecord record() const;
  bool matter_enabled() const;  // false once Member
  const GroupState& state() const;
  // Ms into the blink cycle, in step with the Coordinator on a Member.
  uint32_t blink_phase_ms(uint32_t now_ms) const;
  // When a State or Heartbeat last arrived from the Coordinator.
  uint32_t last_heard_ms() const;
  // The Wi-Fi channel to listen on while hunting for the Coordinator, or 0 to
  // leave the radio where it is. A Member on the pairing channel uses it to find
  // a Coordinator that moved to its router's Wi-Fi channel: after
  // kChannelHuntQuietMs without hearing it, step through channels 1 to
  // kChannelHuntLast, kChannelHuntDwellMs each, until a Heartbeat arrives. A
  // Coordinator, or a Unit alone in its Group, never hunts, and no Unit hunts
  // while a Matter commissioning session is in progress (on_commissioning).
  uint8_t listen_channel(uint32_t now_ms) const;

 private:
  std::vector<Outgoing> to_other_members(MessageType type, const StateSync* sync);
  void realign_blink(const StateSync& sync, uint32_t now_ms);

  Mac self_;
  uint32_t group_id_;
  std::vector<Mac> members_;
  GroupSecret secret_;
  uint32_t boot_nonce_;
  uint32_t counter_ = 0;  // one counter for every message this Unit sends
  SyncRole role_;
  GroupState state_;
  BlinkClock blink_clock_;
  // Coordinator side.
  uint32_t seq_ = 0;
  bool sent_heartbeat_ = false;
  uint32_t last_heartbeat_ms_ = 0;
  // Member side.
  std::optional<Mac> coordinator_;  // restored from the record, else learned from the first accepted message
  bool heard_any_ = false;
  // Freshness of the Coordinator's frames: its current boot nonce and counter,
  // plus the nonces of its earlier boots, which are never accepted again.
  struct Freshness {
    bool nonce_known = false;
    uint32_t current_nonce = 0;
    uint32_t last_counter = 0;
    std::array<uint32_t, kRetiredNonces> retired{};
    size_t retired_count = 0;
    size_t retired_next = 0;
    // Whether a frame is new for its boot; records it when it is.
    bool accept(const SyncAuth& auth);
  };
  Freshness freshness_;
  uint32_t last_seq_ = 0;
  uint32_t last_heard_ms_ = 0;
  bool commissioning_ = false;
};

}  // namespace sls
