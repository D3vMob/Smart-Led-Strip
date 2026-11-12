// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
#include "espnow_codec.h"
#include "sync_auth.h"

namespace sls {

constexpr size_t kMaxMembers = kMaxGroupMembers;
constexpr size_t kMaxHeardPeers = 16;
constexpr uint32_t kPairingWindowMs = 30000;
constexpr uint32_t kHelloIntervalMs = 1000;

struct PairingActions {
  bool window_open = false;
  bool send_hello = false;           // broadcast a PairingHello now
  std::optional<Mac> send_ack_to;    // unicast PairingAck to a newly heard peer
  bool group_formed = false;         // true exactly once, when the window closes
};

class Pairing {
 public:
  // Starts as a Group of one with the window closed.
  explicit Pairing(const Mac& self);
  // Boot after a reboot: restores a persisted group, window closed.
  Pairing(const Mac& self, const std::vector<Mac>& persisted, const GroupSecret& persisted_secret);

  PairingActions on_long_press(uint32_t now_ms, const PairingNonce& nonce);
  PairingActions on_hello(const Mac& from, const PairingInfo& info, uint32_t now_ms);
  PairingActions on_ack(const Mac& from, const PairingInfo& info, uint32_t now_ms);
  PairingActions on_tick(uint32_t now_ms);

  // What this Unit puts in its hellos and acks.
  PairingInfo pairing_info(uint32_t now_ms) const;

  // Secret every member of the formed Group derives alike: SHA-256 over each member's MAC
  // followed by its session nonce, in ascending MAC order. All zero for a Group of one.
  const GroupSecret& group_secret() const { return secret_; }
  bool window_open() const { return window_open_; }
  uint32_t group_id() const;                // derived from the lowest MAC in members()
  const std::vector<Mac>& members() const { return members_; }  // sorted ascending, self included

 private:
  struct Heard {
    Mac mac;
    uint32_t press_ms;
    std::vector<Mac> table;
    PairingNonce nonce;
  };

  bool add_member(const Mac& mac);
  // Returns true when the peer was heard for the first time this session.
  bool hear(const Mac& from, const PairingInfo& info, uint32_t now_ms);
  bool is_closed_out(const Heard& peer) const;
  void recompute_members();
  void derive_secret();

  Mac self_;
  std::vector<Mac> members_;
  bool window_open_ = false;
  uint32_t window_start_ms_ = 0;
  uint32_t session_start_ms_ = 0;
  std::vector<Heard> heard_;
  uint32_t last_hello_ms_ = 0;
  PairingNonce nonce_{};
  GroupSecret secret_{};
};

}  // namespace sls
