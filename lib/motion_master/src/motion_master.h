#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "espnow_codec.h"

namespace sls {

constexpr uint32_t kMotionMasterSilenceMs = 10000;  // master silent longer than this -> failover
constexpr uint8_t kCalibrationOnsets = 10;           // Calibration ends after this many Presence onsets

struct SensorScore {
  Mac mac;
  uint16_t points;
  uint32_t total_presence_ms;
};

// What the Coordinator saves in flash: the Override and the Calibration so far. Cleared on re-pairing.
struct MotionMasterRecord {
  std::optional<Mac> override_mac;
  uint8_t onsets_counted = 0;
  std::vector<SensorScore> scores;
};

class MotionMaster {
 public:
  explicit MotionMaster(const MotionMasterRecord& saved = {});

  // A Sensor unit's presence level: sent on every change and repeated as a keepalive.
  void on_presence_report(const Mac& sensor, bool present, uint32_t total_presence_ms, uint32_t now_ms);
  void on_override(const Mac& sensor, uint32_t now_ms);
  void on_tick(uint32_t now_ms);

  std::optional<Mac> master() const;           // none when no Sensor unit is heard
  std::optional<bool> master_present() const;  // the master's last presence level; none without a master
  std::vector<Mac> ranking() const;            // failover order, best first
  bool calibrating() const;
  MotionMasterRecord record() const;

 private:
  MotionMasterRecord saved_;
  struct Sensor {
    Mac mac;
    bool present = false;
    uint32_t last_heard_ms = 0;
    uint16_t points = 0;
    uint32_t total_presence_ms = 0;
    bool heard = false;  // false for a sensor restored from flash until it reports
  };
  const Sensor* find_master() const;
  Sensor& heard_from(const Mac& mac, uint32_t now_ms);
  bool silent(const Sensor& s) const;
  std::vector<Mac> sorted_by_score() const;
  std::vector<Sensor> sensors_;
  uint32_t now_ms_ = 0;  // latest time given
  std::vector<Mac> frozen_order_;  // ranking at the moment Calibration ended
};

// Flash form of a MotionMasterRecord: a leading format-version byte, then the record.
std::vector<uint8_t> encode_motion_master_record(const MotionMasterRecord& record);
std::optional<MotionMasterRecord> decode_motion_master_record(const uint8_t* data, size_t len);

}  // namespace sls
