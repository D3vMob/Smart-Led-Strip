// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#pragma once
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "blink.h"
#include "channel_scan.h"
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
  // On a Member: the highest boot counter it accepted from the Coordinator.
  uint32_t coordinator_boot = 0;
  // The Wi-Fi channel the Group's radios sit on (the home channel); 0 until one is known, and the
  // pairing channel counts as home meanwhile. Kept across a re-pairing.
  uint8_t channel = 0;
};

// A message to unicast to one Member.
struct Outgoing {
  Mac to;
  Message message;
};

constexpr uint32_t kHeartbeatIntervalMs = 2000;
constexpr uint32_t kBlinkRealignMs = 20;
// A Member that knows its Coordinator and has accepted no State or Heartbeat from it for
// more than this long calls the Coordinator silent (see GroupSync::silent).
constexpr uint32_t kCoordinatorSilenceMs = 10000;

// What a Member that found its Coordinator on a new channel tells the rest of the Group: one signed
// ChannelMoved per other Group member, to send while the radio is still on old_channel.
struct ChannelAnnouncement {
  uint8_t old_channel;
  std::vector<Outgoing> messages;
};

// Keeps every Member of a Group showing the Coordinator's Group state and
// blinking in step. Returns the messages to send instead of sending them.
class GroupSync {
 public:
  // members: the Group table, sorted, self included. secret: the Group secret every
  // sent message is signed with and every received one is verified against.
  // boot_count: this Unit's boot counter, sent in the auth block of every message.
  GroupSync(const Mac& self, uint32_t group_id, std::vector<Mac> members, const GroupSecret& secret,
            const SyncRecord& persisted, uint32_t boot_count, const GroupState& initial);

  // This Unit was commissioned into Matter: it becomes Coordinator and tells
  // every other Member to stay Matter-silent.
  std::vector<Outgoing> on_commissioned(uint32_t now_ms);
  // The Group state changed here (voice, motion). A Coordinator relays it as a
  // State to every other Member; an Undecided Unit only keeps it; a Member
  // ignores it, except a silent one (see silent), which keeps it as its state and sends nothing. An unchanged state sends nothing, so it can be called every loop.
  std::vector<Outgoing> on_local_change(const GroupState& state, uint32_t now_ms);
  // A Coordinator sends a Heartbeat every kHeartbeatIntervalMs: the Group state,
  // its blink phase, the Wi-Fi channel it is on and the Motion master it currently knows (if any).
  std::vector<Outgoing> on_tick(uint32_t now_ms, uint8_t wifi_channel, std::optional<Mac> motion_master = std::nullopt);
  // On a Coordinator, after on_member_report accepted a Presence and it was fed to the election:
  // when sender is the Motion master and it is the first report from a master since this Unit
  // started (coordinator_silent or not), or coordinator_silent is true (the master reported it
  // had lost the Coordinator, or its first report after hearing it again) and the sender differs
  // from the last reporting master or more than kCoordinatorSilenceMs passed since its previous
  // report, the Coordinator adopts open as its Group Open/Closed, bumps its seq and sends a Heartbeat with that state on
  // the next on_tick whatever the interval. Returns true then. Any other report (an unflagged report after the first, a flagged one from the master heard
  // within kCoordinatorSilenceMs, a sender that is not the master, no master, not a Coordinator)
  // returns false and changes nothing.
  bool on_master_presence(const Mac& sender, bool open, bool coordinator_silent, const std::optional<Mac>& master, uint32_t now_ms);
  // State, Heartbeat and MatterSilent from this Group; anything else is ignored.
  // Hearing any of them turns an Undecided Unit into a Member; a Coordinator keeps its role.
  // Accepted only when: the message carries a valid tag for the Group secret (a wrong or
  // missing tag is ignored), this Unit has other Group members, from (the radio source MAC)
  // is a Group member other than this Unit and equals message.sender. A Member
  // learns its Coordinator from the first MatterSilent, State or Heartbeat it
  // accepts (unless the persisted record already names it; save record() when it
  // changes) and from then on ignores all of them from anyone else.
  // The message must also be fresh for the Coordinator: its boot count must be higher than
  // the highest accepted so far (the saved one counts after a restart), or equal to it with a
  // counter newer than the last accepted one (the first counter of a boot since this Unit
  // started is taken as it comes). A lower boot count is a captured frame and is ignored.
  // Ignored messages have no effect at all.
  // A silent Member that knew a Motion master and whose Open/Closed changed locally during the
  // silence (see on_local_change) takes its first State or Heartbeat after the silence with that
  // message's Look and Motion enabled but keeps its own Open/Closed, and keeps doing so for further
  // messages until one with a newer seq than that first one arrives, which applies fully. Otherwise
  // (no local Open/Closed change, or no Motion master known) the first message applies fully.
  // Returns true when the message was applied, false when it was ignored.
  bool on_message(const Message& message, const Mac& from, uint32_t now_ms);

  // A Member that knows its Coordinator: one signed Presence addressed to it, carrying the
  // sensor's reading and this Unit's current Group Open/Closed. While the Coordinator is silent
  // (see silent) the Presence goes to every other Group member instead, the Coordinator included
  // so it hears the return; those, and the first one after the Coordinator is heard again, carry coordinator_silent. A Coordinator (it feeds its own sensor locally) or an Undecided Unit: nothing.
  std::vector<Outgoing> report_presence(bool present, uint32_t total_presence_ms, bool open, uint32_t now_ms);
  // Same routing, an OverrideClaim: this Unit's Sensor asks to become the Motion master.
  std::vector<Outgoing> claim_override();
  // On the Coordinator: a Presence or OverrideClaim from another Group member. Accepted only
  // when it carries a valid tag for the Group secret, from (the radio source MAC) is a Group
  // member other than this Unit and equals message.sender, and it is fresh for that sender:
  // a higher boot count than the last accepted from it, or the same boot count with a newer
  // counter (the first report from a sender since this Unit started is taken as it comes).
  // On a Member: only a Presence, only while silent(now_ms), and only from the Motion master
  // named in the last accepted Heartbeat or State (from is that master), with the same checks
  // and freshness; an OverrideClaim is always ignored. Any other role or type, or a failed
  // check, is ignored with no effect.
  // A Member also accepts a ChannelMoved from any Group member other than this Unit (from is the
  // sender, valid tag, fresh for that sender, channel 1..kLastWifiChannel) and makes that channel
  // its home; the Coordinator ignores it.
  // Returns true when the report was accepted.
  bool on_member_report(const Message& message, const Mac& from, uint32_t now_ms);

  // Matter commissioning is scanning for or joining the router; the radio must
  // not move meanwhile, so listen_channel returns 0 until it stops.
  void on_commissioning(bool in_progress);

  // The Unit was re-paired: a new Group table and Group secret. The role and the
  // Coordinator pin are cleared (record() becomes {Undecided, none}) along with
  // what was heard from the old Coordinator, so the new Group elects its Coordinator again.
  // The Group state and blink clock are kept.
  void regroup(uint32_t group_id, std::vector<Mac> members, const GroupSecret& secret);

  SyncRole role() const;
  // The current role, pinned Coordinator, its highest accepted boot count and the home channel, to save in flash when it changes.
  SyncRecord record() const;
  bool matter_enabled() const;  // false once Member
  const GroupState& state() const;
  // Ms into the blink cycle, in step with the Coordinator on a Member.
  uint32_t blink_phase_ms(uint32_t now_ms) const;
  // When a State or Heartbeat last arrived from the Coordinator.
  uint32_t last_heard_ms() const;
  // True on a Member that knows its Coordinator when no State or Heartbeat from it has been
  // accepted for more than kCoordinatorSilenceMs (counted from 0 when none was heard since
  // this Unit started). Never true on a Coordinator or an Undecided Unit.
  bool silent(uint32_t now_ms) const;
  // On a Member: the Motion master named in the last accepted State or Heartbeat, if any.
  std::optional<Mac> motion_master() const;
  // The Wi-Fi channel to listen on, or 0 to leave the radio where it is. A Coordinator, a Unit
  // alone in its Group and any Unit while a Matter commissioning session is in progress
  // (on_commissioning) get 0. Any other Unit (a Member, or an Undecided one with other Group
  // members) listens on its home channel (record().channel; the pairing channel when none is known
  // yet) while it hears the Coordinator. Once nothing (State or Heartbeat) was heard for more than
  // kCoordinatorSilenceMs it scans: scan_channel(home, time since silence began).
  uint8_t listen_channel(uint32_t now_ms) const;
  // Set when a silent Member accepts a Heartbeat carrying a Wi-Fi channel other than its home:
  // the home becomes that channel and this returns, once, the ChannelMoved messages for every
  // other Group member to send on the old channel before the radio moves. Later calls return none.
  // A Member that was not silent just moves its home, with no announcement.
  std::optional<ChannelAnnouncement> channel_announcement();

 private:
  std::vector<Outgoing> to_other_members(MessageType type, const StateSync* sync);
  std::vector<Outgoing> to_other_members(const Message& body);
  std::vector<Outgoing> to_coordinator(const Message& body);
  void realign_blink(const StateSync& sync, uint32_t now_ms);
  // Whether this role takes this kind of report from this sender now, before any tag or freshness check.
  bool wants_report(const Message& message, const Mac& from, uint32_t now_ms) const;

  Mac self_;
  uint32_t group_id_;
  std::vector<Mac> members_;
  GroupSecret secret_;
  uint32_t boot_count_;
  uint32_t counter_ = 0;  // one counter for every message this Unit sends
  SyncRole role_;
  GroupState state_;
  BlinkClock blink_clock_;
  // Coordinator side.
  uint32_t seq_ = 0;
  bool sent_heartbeat_ = false;
  bool heartbeat_due_ = false;  // an adopted master state is sent without waiting for the interval
  bool reported_while_silent_ = false;  // the last Presence report went out while the Coordinator was silent
  std::optional<std::pair<Mac, uint32_t>> master_report_;  // the Motion master and when it last reported
  uint32_t last_heartbeat_ms_ = 0;
  // Member side.
  std::optional<Mac> coordinator_;  // restored from the record, else learned from the first accepted message
  bool heard_any_ = false;
  // Set when the Coordinator returned after a silence during which a Motion master was known: the
  // Member keeps its own Open/Closed until a message newer than return_seq_ arrives.
  bool keeping_open_ = false;
  bool open_changed_locally_ = false;  // Open/Closed changed here (on_local_change) during the current silence
  uint32_t return_seq_ = 0;
  std::optional<Mac> motion_master_;
  // Freshness of the Coordinator's frames: the highest boot count accepted (restored from
  // the record) and the last counter accepted for it since this Unit started.
  bool fresh(const SyncAuth& auth) const;
  uint32_t coordinator_boot_ = 0;
  uint8_t home_channel_ = 0;  // 0 = none known yet
  std::optional<ChannelAnnouncement> announcement_;
  uint8_t home() const;
  bool counter_known_ = false;
  uint32_t last_counter_ = 0;
  // Coordinator side: the last (boot count, counter) accepted from each Member's reports.
  std::vector<std::pair<Mac, SyncAuth>> last_report_;
  uint32_t last_seq_ = 0;
  uint32_t last_heard_ms_ = 0;
  bool commissioning_ = false;
};

}  // namespace sls
