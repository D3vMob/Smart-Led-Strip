#include "pairing.h"

#include <algorithm>

namespace sls {

Pairing::Pairing(const Mac& self) : self_(self), members_{self} {}

Pairing::Pairing(const Mac& self, const std::vector<Mac>& persisted)
    : self_(self), members_{self} {
  for (const Mac& mac : persisted) add_member(mac);
}

PairingActions Pairing::on_long_press(uint32_t now_ms) {
  if (!window_open_) members_ = {self_};
  window_open_ = true;
  window_start_ms_ = now_ms;
  last_hello_ms_ = now_ms;
  PairingActions actions;
  actions.window_open = true;
  actions.send_hello = true;
  return actions;
}

PairingActions Pairing::on_tick(uint32_t now_ms) {
  PairingActions actions;
  if (window_open_ && now_ms - window_start_ms_ >= kPairingWindowMs) {
    window_open_ = false;
    actions.group_formed = true;
  }
  actions.window_open = window_open_;
  if (window_open_ && now_ms - last_hello_ms_ >= kHelloIntervalMs) {
    last_hello_ms_ = now_ms;
    actions.send_hello = true;
  }
  return actions;
}

PairingActions Pairing::on_hello(const Mac& from, uint32_t now_ms) {
  (void)now_ms;
  PairingActions actions;
  actions.window_open = window_open_;
  if (window_open_ && add_member(from)) {
    actions.send_ack_to = from;
  }
  return actions;
}

PairingActions Pairing::on_ack(const Mac& from, uint32_t now_ms) {
  (void)now_ms;
  PairingActions actions;
  actions.window_open = window_open_;
  if (window_open_) add_member(from);
  return actions;
}

uint32_t Pairing::group_id() const {
  uint32_t hash = 2166136261u;
  for (uint8_t byte : members_.front()) {
    hash ^= byte;
    hash *= 16777619u;
  }
  return hash == 0 ? 1 : hash;
}

bool Pairing::add_member(const Mac& mac) {
  auto pos = std::lower_bound(members_.begin(), members_.end(), mac);
  if (pos != members_.end() && *pos == mac) return false;
  if (members_.size() >= kMaxMembers) return false;
  members_.insert(pos, mac);
  return true;
}

}  // namespace sls
