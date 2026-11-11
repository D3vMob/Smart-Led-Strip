#include "group_sync.h"

#include <algorithm>
#include <utility>

#include "config.h"

namespace sls {

namespace {
// Whether two Group states would look and behave the same.
bool same_state(const GroupState& a, const GroupState& b) {
  return a.open == b.open && a.motion_enabled == b.motion_enabled && a.look == b.look;
}
}  // namespace

GroupSync::GroupSync(const Mac& self, uint32_t group_id, std::vector<Mac> members, const GroupSecret& secret,
                     const SyncRecord& persisted, uint32_t boot_count, const GroupState& initial)
    : self_(self),
      group_id_(group_id),
      members_(std::move(members)),
      secret_(secret),
      boot_count_(boot_count),
      role_(persisted.role),
      state_(initial),
      coordinator_(persisted.role == SyncRole::Member ? persisted.coordinator : std::nullopt),
      coordinator_boot_(persisted.role == SyncRole::Member ? persisted.coordinator_boot : 0),
      home_channel_(persisted.channel >= 1 && persisted.channel <= kLastWifiChannel ? persisted.channel : 0) {}

std::vector<Outgoing> GroupSync::to_other_members(MessageType type, const StateSync* sync) {
  Message message{type, group_id_, self_, std::monostate{}};
  if (sync != nullptr) message.body = *sync;
  return to_other_members(message);
}

std::vector<Outgoing> GroupSync::to_other_members(const Message& body) {
  std::vector<Outgoing> out;
  for (const Mac& member : members_) {
    if (member == self_) continue;
    Message message = body;
    message.auth.boot_count = boot_count_;
    message.auth.counter = ++counter_;
    sign(message, secret_);
    out.push_back({member, message});
  }
  return out;
}

std::vector<Outgoing> GroupSync::on_commissioned(uint32_t) {
  role_ = SyncRole::Coordinator;
  return to_other_members(MessageType::MatterSilent, nullptr);
}

std::vector<Outgoing> GroupSync::on_local_change(const GroupState& state, uint32_t now_ms) {
  if (role_ == SyncRole::Member) {
    if (silent(now_ms)) {  // keeps the last state and its motion changes, relays nothing
      if (state.open != state_.open) open_changed_locally_ = true;
      state_ = state;
    }
    return {};
  }
  if (same_state(state, state_)) return {};
  state_ = state;
  if (role_ != SyncRole::Coordinator) return {};
  ++seq_;
  const StateSync sync{state_, seq_, 0, 0};
  return to_other_members(MessageType::State, &sync);
}

std::vector<Outgoing> GroupSync::on_tick(uint32_t now_ms, uint8_t wifi_channel, std::optional<Mac> motion_master) {
  if (role_ != SyncRole::Coordinator) return {};
  if (!heartbeat_due_ && sent_heartbeat_ && now_ms - last_heartbeat_ms_ < kHeartbeatIntervalMs) return {};
  heartbeat_due_ = false;
  sent_heartbeat_ = true;
  last_heartbeat_ms_ = now_ms;
  const StateSync sync{state_, seq_, blink_phase_ms(now_ms), wifi_channel, motion_master};
  return to_other_members(MessageType::Heartbeat, &sync);
}

void GroupSync::realign_blink(const StateSync& sync, uint32_t now_ms) {
  const uint32_t period = blink_period_ms(sync.state.look.blink_speed_pct);
  const uint32_t mine = blink_clock_.phase_ms(now_ms, period);
  const uint32_t theirs = sync.blink_phase_ms % period;
  const uint32_t apart = mine > theirs ? mine - theirs : theirs - mine;
  const uint32_t drift = apart < period - apart ? apart : period - apart;
  if (drift > kBlinkRealignMs) blink_clock_.align(now_ms, theirs);
}

bool GroupSync::fresh(const SyncAuth& auth) const {
  if (auth.boot_count > coordinator_boot_) return true;
  if (auth.boot_count < coordinator_boot_) return false;
  return !counter_known_ || seq_is_newer(auth.counter, last_counter_);
}

bool GroupSync::on_message(const Message& message, const Mac& from, uint32_t now_ms) {
  if (!verify(message, secret_)) return false;  // forged, corrupted or unsigned: no effect at all
  if (members_.size() <= 1) return false;  // a Unit alone in its Group has no one to sync with
  if (message.group_id != group_id_) return false;
  if (from == self_ || std::find(members_.begin(), members_.end(), from) == members_.end()) return false;
  if (from != message.sender) return false;  // the radio source must be the claimed sender
  if (role_ == SyncRole::Coordinator) return false;  // the Coordinator owns the Group state
  if (coordinator_ && *coordinator_ != from) return false;  // only the Coordinator we follow
  if (message.type != MessageType::MatterSilent && !std::get_if<StateSync>(&message.body)) return false;
  const bool was_silent = silent(now_ms);
  if (!fresh(message.auth)) return false;  // a replayed or earlier-boot frame: no effect at all
  coordinator_boot_ = message.auth.boot_count;
  last_counter_ = message.auth.counter;
  counter_known_ = true;
  if (message.type == MessageType::MatterSilent) {
    coordinator_ = from;
    role_ = SyncRole::Member;
    return true;
  }
  const StateSync* sync = std::get_if<StateSync>(&message.body);
  coordinator_ = from;
  role_ = SyncRole::Member;  // hearing the Coordinator settles an Undecided Unit
  last_heard_ms_ = now_ms;
  // A Heartbeat is the Coordinator's current truth, so it always applies, which
  // also lets Members follow a Coordinator whose sequence restarted on reboot.
  // A State applies only when it is newer than the last one (latest wins).
  if (message.type == MessageType::Heartbeat) {
    realign_blink(*sync, now_ms);
  } else if (heard_any_ && !seq_is_newer(sync->seq, last_seq_)) {
    return true;  // stale, but it still counted as hearing the Coordinator
  }
  heard_any_ = true;
  last_seq_ = sync->seq;
  if (was_silent && motion_master_ && open_changed_locally_) {
    keeping_open_ = true;  // Open/Closed changed here while the Coordinator was away
    return_seq_ = sync->seq;
  } else if (keeping_open_ && seq_is_newer(sync->seq, return_seq_)) {
    keeping_open_ = false;
  }
  open_changed_locally_ = false;
  const bool own_open = state_.open;
  state_ = sync->state;
  if (keeping_open_) state_.open = own_open;
  motion_master_ = sync->motion_master;
  if (message.type == MessageType::Heartbeat && sync->wifi_channel >= 1 && sync->wifi_channel <= kLastWifiChannel) {
    if (was_silent && sync->wifi_channel != home()) {
      announcement_ = ChannelAnnouncement{
          home(), to_other_members(Message{MessageType::ChannelMoved, group_id_, self_, ChannelMoved{sync->wifi_channel}})};
    }
    home_channel_ = sync->wifi_channel;
  }
  return true;
}

bool GroupSync::on_master_presence(const Mac& sender, bool open, bool coordinator_silent,
                                   const std::optional<Mac>& master, uint32_t now_ms) {
  if (role_ != SyncRole::Coordinator || !master || sender != *master) return false;
  const bool first = !master_report_;
  const bool new_report = first || master_report_->first != sender ||
                          now_ms - master_report_->second > kCoordinatorSilenceMs;
  master_report_ = std::make_pair(sender, now_ms);
  if (!first && !(coordinator_silent && new_report)) return false;
  state_.open = open;
  ++seq_;
  heartbeat_due_ = true;
  return true;
}

std::vector<Outgoing> GroupSync::to_coordinator(const Message& body) {
  if (role_ != SyncRole::Member || !coordinator_) return {};
  Message message = body;
  message.auth.boot_count = boot_count_;
  message.auth.counter = ++counter_;
  sign(message, secret_);
  return {{*coordinator_, message}};
}

std::vector<Outgoing> GroupSync::report_presence(bool present, uint32_t total_presence_ms, bool open,
                                              uint32_t now_ms) {
  const bool is_silent = silent(now_ms);
  const Presence presence{present, total_presence_ms, open, is_silent || reported_while_silent_};
  reported_while_silent_ = is_silent;
  if (is_silent) {
    return to_other_members(Message{MessageType::Presence, group_id_, self_, presence});
  }
  return to_coordinator(Message{MessageType::Presence, group_id_, self_, presence});
}

std::vector<Outgoing> GroupSync::claim_override() {
  return to_coordinator(Message{MessageType::OverrideClaim, group_id_, self_, std::monostate{}});
}

bool GroupSync::wants_report(const Message& message, const Mac& from, uint32_t now_ms) const {
  switch (message.type) {
    case MessageType::Presence:
      if (role_ == SyncRole::Coordinator) return true;
      // A silent Member follows the Motion master named in the Coordinator's last Heartbeat.
      return role_ == SyncRole::Member && silent(now_ms) && motion_master_ && from == *motion_master_;
    case MessageType::OverrideClaim:
      return role_ == SyncRole::Coordinator;
    case MessageType::ChannelMoved: {
      const ChannelMoved* moved = std::get_if<ChannelMoved>(&message.body);
      return role_ == SyncRole::Member && moved != nullptr && moved->channel >= 1 &&
             moved->channel <= kLastWifiChannel;
    }
    default:
      return false;
  }
}

bool GroupSync::on_member_report(const Message& message, const Mac& from, uint32_t now_ms) {
  if (!wants_report(message, from, now_ms)) return false;
  if (!verify(message, secret_)) return false;
  if (message.group_id != group_id_) return false;
  if (from == self_ || std::find(members_.begin(), members_.end(), from) == members_.end()) return false;
  if (from != message.sender) return false;
  auto last = std::find_if(last_report_.begin(), last_report_.end(), [&](const auto& e) { return e.first == from; });
  if (last != last_report_.end()) {
    const SyncAuth& seen = last->second;
    const bool newer = message.auth.boot_count > seen.boot_count ||
                       (message.auth.boot_count == seen.boot_count && seq_is_newer(message.auth.counter, seen.counter));
    if (!newer) return false;
    last->second = message.auth;
  } else {
    last_report_.push_back({from, message.auth});
  }
  if (const ChannelMoved* moved = std::get_if<ChannelMoved>(&message.body)) home_channel_ = moved->channel;
  return true;
}

void GroupSync::regroup(uint32_t group_id, std::vector<Mac> members, const GroupSecret& secret) {
  group_id_ = group_id;
  members_ = std::move(members);
  secret_ = secret;
  role_ = SyncRole::Undecided;
  seq_ = 0;
  heartbeat_due_ = false;
  master_report_.reset();
  sent_heartbeat_ = false;
  last_heartbeat_ms_ = 0;
  coordinator_.reset();
  heard_any_ = false;
  keeping_open_ = false;
  open_changed_locally_ = false;
  motion_master_.reset();
  coordinator_boot_ = 0;
  counter_known_ = false;
  last_counter_ = 0;
  last_report_.clear();
  last_seq_ = 0;
  last_heard_ms_ = 0;
}

SyncRole GroupSync::role() const { return role_; }
SyncRecord GroupSync::record() const {
  if (role_ != SyncRole::Member) return {role_, std::nullopt, 0, home_channel_};
  return {role_, coordinator_, coordinator_boot_, home_channel_};
}
bool GroupSync::matter_enabled() const { return role_ != SyncRole::Member; }
const GroupState& GroupSync::state() const { return state_; }
uint32_t GroupSync::blink_phase_ms(uint32_t now_ms) const {
  return blink_clock_.phase_ms(now_ms, blink_period_ms(state_.look.blink_speed_pct));
}
uint32_t GroupSync::last_heard_ms() const { return last_heard_ms_; }
bool GroupSync::silent(uint32_t now_ms) const {
  if (role_ != SyncRole::Member || !coordinator_) return false;
  return now_ms - last_heard_ms_ > kCoordinatorSilenceMs;
}
std::optional<Mac> GroupSync::motion_master() const { return motion_master_; }
void GroupSync::on_commissioning(bool in_progress) { commissioning_ = in_progress; }
uint8_t GroupSync::home() const { return home_channel_ != 0 ? home_channel_ : ESPNOW_PAIRING_CHANNEL; }
std::optional<ChannelAnnouncement> GroupSync::channel_announcement() {
  std::optional<ChannelAnnouncement> out = std::move(announcement_);
  announcement_.reset();
  return out;
}
uint8_t GroupSync::listen_channel(uint32_t now_ms) const {
  if (commissioning_ || role_ == SyncRole::Coordinator || members_.size() <= 1) return 0;
  const uint8_t home = this->home();
  const uint32_t quiet = now_ms - last_heard_ms_;
  if (quiet <= kCoordinatorSilenceMs) return home;
  return scan_channel(home, quiet - kCoordinatorSilenceMs);
}

}  // namespace sls
