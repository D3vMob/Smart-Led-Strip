#include "pairing.h"

#include <algorithm>

namespace sls {

namespace {

// Press order: the earlier press ranks first (wrap-safe), the lower MAC breaks a tie.
bool ranks_before(uint32_t press_a, const Mac& mac_a, uint32_t press_b, const Mac& mac_b) {
  const int32_t diff = int32_t(press_a - press_b);
  return diff != 0 ? diff < 0 : mac_a < mac_b;
}

}  // namespace

Pairing::Pairing(const Mac& self) : self_(self), members_{self} {}

Pairing::Pairing(const Mac& self, const std::vector<Mac>& persisted, const GroupSecret& persisted_secret)
    : self_(self), members_{self}, secret_(persisted_secret) {
  for (const Mac& mac : persisted) add_member(mac);
}

PairingActions Pairing::on_long_press(uint32_t now_ms, const PairingNonce& nonce) {
  if (!window_open_) {
    members_ = {self_};
    heard_.clear();
    session_start_ms_ = now_ms;
    nonce_ = nonce;
  }
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
    derive_secret();
    actions.group_formed = true;
  }
  actions.window_open = window_open_;
  if (window_open_ && now_ms - last_hello_ms_ >= kHelloIntervalMs) {
    last_hello_ms_ = now_ms;
    actions.send_hello = true;
  }
  return actions;
}

PairingActions Pairing::on_hello(const Mac& from, const PairingInfo& info, uint32_t now_ms) {
  PairingActions actions;
  actions.window_open = window_open_;
  if (window_open_ && hear(from, info, now_ms) &&
      std::find(members_.begin(), members_.end(), from) != members_.end()) {
    actions.send_ack_to = from;
  }
  return actions;
}

PairingActions Pairing::on_ack(const Mac& from, const PairingInfo& info, uint32_t now_ms) {
  PairingActions actions;
  actions.window_open = window_open_;
  if (window_open_) hear(from, info, now_ms);
  return actions;
}

PairingInfo Pairing::pairing_info(uint32_t now_ms) const {
  return PairingInfo{now_ms - session_start_ms_, members_, nonce_};
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

bool Pairing::hear(const Mac& from, const PairingInfo& info, uint32_t now_ms) {
  const uint32_t press_ms = now_ms - info.window_age_ms;
  auto found = std::find_if(heard_.begin(), heard_.end(), [&](const Heard& h) { return h.mac == from; });
  const bool first = found == heard_.end();
  if (first) {
    if (heard_.size() >= kMaxHeardPeers) return false;
    heard_.push_back({from, press_ms, info.members, info.nonce});
  } else {
    found->press_ms = press_ms;
    found->table = info.members;
    found->nonce = info.nonce;
  }
  recompute_members();
  return first;
}

// A peer whose latest table is full, lacks this Unit and holds only Units that
// rank before it has already settled a Group this Unit cannot join. A Unit in
// that table this Unit never heard counts as ranking before it.
bool Pairing::is_closed_out(const Heard& peer) const {
  if (peer.table.size() < kMaxMembers) return false;
  if (std::find(peer.table.begin(), peer.table.end(), self_) != peer.table.end()) return false;
  for (const Mac& mac : peer.table) {
    auto heard = std::find_if(heard_.begin(), heard_.end(), [&](const Heard& h) { return h.mac == mac; });
    if (heard == heard_.end()) continue;
    if (!ranks_before(heard->press_ms, heard->mac, session_start_ms_, self_)) return false;
  }
  return true;
}

void Pairing::derive_secret() {
  std::vector<uint8_t> material;
  for (const Mac& mac : members_) {
    material.insert(material.end(), mac.begin(), mac.end());
    const PairingNonce* nonce = &nonce_;
    for (const Heard& h : heard_) {
      if (h.mac == mac) nonce = &h.nonce;
    }
    material.insert(material.end(), nonce->begin(), nonce->end());
  }
  secret_ = members_.size() > 1 ? sha256(material.data(), material.size()) : GroupSecret{};
}

void Pairing::recompute_members() {
  struct Entry {
    Mac mac;
    uint32_t press_ms;
  };
  std::vector<Entry> ranked{{self_, session_start_ms_}};
  for (const Heard& h : heard_) {
    if (!is_closed_out(h)) ranked.push_back({h.mac, h.press_ms});
  }
  std::sort(ranked.begin(), ranked.end(), [](const Entry& a, const Entry& b) {
    return ranks_before(a.press_ms, a.mac, b.press_ms, b.mac);
  });
  if (ranked.size() > kMaxMembers) ranked.resize(kMaxMembers);
  members_.clear();
  for (const Entry& e : ranked) members_.push_back(e.mac);
  std::sort(members_.begin(), members_.end());
  if (std::find(members_.begin(), members_.end(), self_) == members_.end()) members_ = {self_};
}

}  // namespace sls
