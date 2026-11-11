#include "motion_master.h"

#include <algorithm>

namespace sls {

bool MotionMaster::ranks_before(const Sensor& a, const Sensor& b) {
  if (a.points != b.points) return a.points > b.points;
  if (a.total_presence_ms != b.total_presence_ms) return a.total_presence_ms > b.total_presence_ms;
  return a.mac < b.mac;
}

MotionMaster::MotionMaster(const MotionMasterRecord& saved) : saved_(saved) {
  for (const SensorScore& score : saved.scores) {
    sensors_.push_back(Sensor{score.mac, false, 0, score.points, score.total_presence_ms, false});
    if (!calibrating()) frozen_order_.push_back(score.mac);
  }
  saved_.scores.clear();
}

MotionMaster::Sensor& MotionMaster::heard_from(const Mac& mac, uint32_t now_ms) {
  now_ms_ = now_ms;
  Sensor* s = nullptr;
  for (auto& x : sensors_) {
    if (x.mac == mac) s = &x;
  }
  if (!s) {
    sensors_.push_back(Sensor{mac});
    s = &sensors_.back();
  }
  s->last_heard_ms = now_ms;
  s->heard = true;
  return *s;
}

void MotionMaster::on_presence_report(const Mac& sensor, bool present, uint32_t total_presence_ms,
                                      uint32_t now_ms) {
  Sensor& s = heard_from(sensor, now_ms);
  s.total_presence_ms = total_presence_ms;
  bool group_present = false;
  for (const auto& x : sensors_) group_present = group_present || (x.present && !silent(x));
  if (present && !s.present && !group_present && calibrating()) {
    ++s.points;
    ++saved_.onsets_counted;
    if (!calibrating()) frozen_order_ = sorted_by_score();
  }
  s.present = present;
}

void MotionMaster::on_override(const Mac& sensor, uint32_t now_ms) {
  heard_from(sensor, now_ms);
  if (calibrating()) frozen_order_ = sorted_by_score();
  saved_.override_mac = sensor;
}
void MotionMaster::on_tick(uint32_t now_ms) { now_ms_ = now_ms; }

bool MotionMaster::silent(const Sensor& s) const {
  return !s.heard || now_ms_ - s.last_heard_ms > kMotionMasterSilenceMs;
}

const MotionMaster::Sensor* MotionMaster::find_master() const {
  if (saved_.override_mac) {
    for (const auto& s : sensors_) {
      if (s.mac == *saved_.override_mac && s.heard) return &s;
    }
  }
  if (calibrating()) {
    const Sensor* lowest = nullptr;
    for (const auto& s : sensors_) {
      if (!silent(s) && (!lowest || s.mac < lowest->mac)) lowest = &s;
    }
    return lowest;
  }
  for (const Mac& mac : ranking()) {
    for (const auto& s : sensors_) {
      if (s.mac == mac && !silent(s)) return &s;
    }
  }
  return nullptr;
}

std::optional<Mac> MotionMaster::master() const {
  const Sensor* m = find_master();
  if (!m) return std::nullopt;
  return m->mac;
}

std::optional<bool> MotionMaster::master_present() const {
  const Sensor* m = find_master();
  if (!m) return std::nullopt;
  return m->present;
}

std::optional<bool> MotionMaster::motion_presence() const {
  if (sensors_.empty()) return std::nullopt;
  const Sensor* m = find_master();
  return m ? m->present : false;
}

std::vector<Mac> MotionMaster::sorted_by_score() const {
  std::vector<Sensor> sorted = sensors_;
  std::sort(sorted.begin(), sorted.end(), ranks_before);
  std::vector<Mac> out;
  for (const auto& s : sorted) out.push_back(s.mac);
  return out;
}

std::vector<Mac> MotionMaster::ranking() const {
  if (calibrating()) return sorted_by_score();
  std::vector<Mac> out = frozen_order_;
  for (const auto& s : sensors_) {
    if (std::find(out.begin(), out.end(), s.mac) == out.end()) out.push_back(s.mac);
  }
  if (saved_.override_mac) {
    out.erase(std::remove(out.begin(), out.end(), *saved_.override_mac), out.end());
    out.insert(out.begin(), *saved_.override_mac);
  }
  return out;
}

bool MotionMaster::calibrating() const {
  return !saved_.override_mac && saved_.onsets_counted < kCalibrationOnsets;
}
MotionMasterRecord MotionMaster::record() const {
  MotionMasterRecord out = saved_;
  out.scores.clear();
  for (const Mac& mac : ranking()) {
    for (const auto& s : sensors_) {
      if (s.mac == mac) out.scores.push_back(SensorScore{s.mac, s.points, s.total_presence_ms});
    }
  }
  return out;
}

namespace {
constexpr uint8_t kRecordFormatVersion = 1;
constexpr size_t kRecordHeaderBytes = 10;  // version, onsets, has-override, override MAC, score count
constexpr size_t kScoreBytes = 12;         // MAC, points (u16), total presence (u32), little-endian
}  // namespace

std::vector<uint8_t> encode_motion_master_record(const MotionMasterRecord& record) {
  std::vector<uint8_t> out;
  out.push_back(kRecordFormatVersion);
  out.push_back(record.onsets_counted);
  out.push_back(record.override_mac ? 1 : 0);
  const Mac override_mac = record.override_mac.value_or(Mac{});
  out.insert(out.end(), override_mac.begin(), override_mac.end());
  out.push_back(static_cast<uint8_t>(record.scores.size()));
  for (const SensorScore& s : record.scores) {
    out.insert(out.end(), s.mac.begin(), s.mac.end());
    for (int i = 0; i < 2; ++i) out.push_back(static_cast<uint8_t>(s.points >> (8 * i)));
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(s.total_presence_ms >> (8 * i)));
  }
  return out;
}

std::optional<MotionMasterRecord> decode_motion_master_record(const uint8_t* data, size_t len) {
  if (!data || len < kRecordHeaderBytes || data[0] != kRecordFormatVersion) return std::nullopt;
  if (data[2] > 1) return std::nullopt;
  const size_t count = data[9];
  if (count > kMaxGroupMembers || len != kRecordHeaderBytes + count * kScoreBytes) return std::nullopt;
  MotionMasterRecord record;
  record.onsets_counted = data[1];
  if (data[2] == 1) {
    Mac mac;
    std::copy(data + 3, data + 9, mac.begin());
    record.override_mac = mac;
  }
  const uint8_t* p = data + kRecordHeaderBytes;
  for (size_t n = 0; n < count; ++n, p += kScoreBytes) {
    SensorScore s{};
    std::copy(p, p + 6, s.mac.begin());
    s.points = static_cast<uint16_t>(p[6] | (p[7] << 8));
    s.total_presence_ms = static_cast<uint32_t>(p[8]) | (static_cast<uint32_t>(p[9]) << 8) |
                          (static_cast<uint32_t>(p[10]) << 16) | (static_cast<uint32_t>(p[11]) << 24);
    record.scores.push_back(s);
  }
  return record;
}

}  // namespace sls
