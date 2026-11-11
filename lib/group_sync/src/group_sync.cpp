#include "group_sync.h"

#include <algorithm>
#include <utility>

namespace sls {

namespace {
// Whether two Group states would look and behave the same.
bool same_state(const GroupState& a, const GroupState& b) {
  return a.open == b.open && a.motion_enabled == b.motion_enabled && a.look.color_mode == b.look.color_mode &&
         a.look.hue_deg == b.look.hue_deg && a.look.saturation == b.look.saturation &&
         a.look.color_temp_mireds == b.look.color_temp_mireds && a.look.brightness_pct == b.look.brightness_pct &&
         a.look.blink_on == b.look.blink_on && a.look.blink_speed_pct == b.look.blink_speed_pct;
}
}  // namespace

GroupSync::GroupSync(const Mac& self, uint32_t group_id, std::vector<Mac> members, SyncRole persisted, const GroupState& initial)
    : self_(self), group_id_(group_id), members_(std::move(members)), role_(persisted), state_(initial) {}

std::vector<Outgoing> GroupSync::to_other_members(MessageType type, const StateSync* sync) const {
  std::vector<Outgoing> out;
  for (const Mac& member : members_) {
    if (member == self_) continue;
    Message message{type, group_id_, self_, std::monostate{}};
    if (sync != nullptr) message.body = *sync;
    out.push_back({member, message});
  }
  return out;
}

std::vector<Outgoing> GroupSync::on_commissioned(uint32_t) {
  role_ = SyncRole::Coordinator;
  return to_other_members(MessageType::MatterSilent, nullptr);
}

std::vector<Outgoing> GroupSync::on_local_change(const GroupState& state, uint32_t) {
  if (role_ == SyncRole::Member) return {};
  if (same_state(state, state_)) return {};
  state_ = state;
  if (role_ != SyncRole::Coordinator) return {};
  ++seq_;
  const StateSync sync{state_, seq_, 0, 0};
  return to_other_members(MessageType::State, &sync);
}

std::vector<Outgoing> GroupSync::on_tick(uint32_t now_ms, uint8_t wifi_channel) {
  if (role_ != SyncRole::Coordinator) return {};
  if (sent_heartbeat_ && now_ms - last_heartbeat_ms_ < kHeartbeatIntervalMs) return {};
  sent_heartbeat_ = true;
  last_heartbeat_ms_ = now_ms;
  const StateSync sync{state_, seq_, blink_phase_ms(now_ms), wifi_channel};
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

bool GroupSync::on_message(const Message& message, const Mac& from, uint32_t now_ms) {
  if (members_.size() <= 1) return false;  // a Unit alone in its Group has no one to sync with
  if (message.group_id != group_id_) return false;
  if (from == self_ || std::find(members_.begin(), members_.end(), from) == members_.end()) return false;
  if (from != message.sender) return false;  // the radio source must be the claimed sender
  if (role_ == SyncRole::Coordinator) return false;  // the Coordinator owns the Group state
  if (coordinator_ && *coordinator_ != from) return false;  // only the Coordinator we follow
  if (message.type == MessageType::MatterSilent) {
    coordinator_ = from;
    role_ = SyncRole::Member;
    return true;
  }
  if (message.type != MessageType::State && message.type != MessageType::Heartbeat) return false;
  const StateSync* sync = std::get_if<StateSync>(&message.body);
  if (!sync) return false;
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
  state_ = sync->state;
  return true;
}

SyncRole GroupSync::role() const { return role_; }
bool GroupSync::matter_enabled() const { return role_ != SyncRole::Member; }
const GroupState& GroupSync::state() const { return state_; }
uint32_t GroupSync::blink_phase_ms(uint32_t now_ms) const {
  return blink_clock_.phase_ms(now_ms, blink_period_ms(state_.look.blink_speed_pct));
}
uint32_t GroupSync::last_heard_ms() const { return last_heard_ms_; }
void GroupSync::on_commissioning(bool in_progress) { commissioning_ = in_progress; }
uint8_t GroupSync::listen_channel(uint32_t now_ms) const {
  if (commissioning_ || role_ == SyncRole::Coordinator || members_.size() <= 1) return 0;
  const uint32_t quiet = now_ms - last_heard_ms_;
  if (quiet < kChannelHuntQuietMs) return 0;
  const uint32_t hunting = quiet - kChannelHuntQuietMs;
  return static_cast<uint8_t>(1 + (hunting / kChannelHuntDwellMs) % kChannelHuntLast);
}

}  // namespace sls
