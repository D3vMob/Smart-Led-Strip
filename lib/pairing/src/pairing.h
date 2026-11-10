#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
#include "espnow_codec.h"

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
  Pairing(const Mac& self, const std::vector<Mac>& persisted);

  PairingActions on_long_press(uint32_t now_ms);
  PairingActions on_hello(const Mac& from, const PairingInfo& info, uint32_t now_ms);
  PairingActions on_ack(const Mac& from, const PairingInfo& info, uint32_t now_ms);
  PairingActions on_tick(uint32_t now_ms);

  // What this Unit puts in its hellos and acks.
  PairingInfo pairing_info(uint32_t now_ms) const;

  bool window_open() const { return window_open_; }
  uint32_t group_id() const;                // derived from the lowest MAC in members()
  const std::vector<Mac>& members() const { return members_; }  // sorted ascending, self included

 private:
  struct Heard {
    Mac mac;
    uint32_t press_ms;
    std::vector<Mac> table;
  };

  bool add_member(const Mac& mac);
  // Returns true when the peer was heard for the first time this session.
  bool hear(const Mac& from, const PairingInfo& info, uint32_t now_ms);
  bool is_closed_out(const Heard& peer) const;
  void recompute_members();

  Mac self_;
  std::vector<Mac> members_;
  bool window_open_ = false;
  uint32_t window_start_ms_ = 0;
  uint32_t session_start_ms_ = 0;
  std::vector<Heard> heard_;
  uint32_t last_hello_ms_ = 0;
};

}  // namespace sls
