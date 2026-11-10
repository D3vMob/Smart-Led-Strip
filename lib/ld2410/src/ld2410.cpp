#include "ld2410.h"

namespace sls {
namespace ld2410 {

namespace {
const uint8_t kReportHeader[4] = {0xF4, 0xF3, 0xF2, 0xF1};
const uint8_t kReportFooter[4] = {0xF8, 0xF7, 0xF6, 0xF5};
const uint8_t kAckHeader[4] = {0xFD, 0xFC, 0xFB, 0xFA};
const uint8_t kAckFooter[4] = {0x04, 0x03, 0x02, 0x01};
const uint8_t kPrefixLen = 6;  // header 4 + length 2
const uint8_t kFooterLen = 4;
const uint8_t kHeadMarker = 0xAA;
const uint8_t kTailMarker = 0x55;
const uint16_t kMinAckDataLen = 4;      // command word + status word
const uint16_t kMinReportDataLen = 13;  // basic report

uint16_t le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

Report decode_report(const uint8_t* d) {
  Report r;
  r.target = static_cast<TargetState>(d[2]);
  r.moving_distance_cm = le16(d + 3);
  r.moving_energy = d[5];
  r.still_distance_cm = le16(d + 6);
  r.still_energy = d[8];
  r.detection_distance_cm = le16(d + 9);
  return r;
}
}  // namespace

bool FrameParser::feed(uint8_t byte, Frame& out) {
  if (len_ == 0) {
    if (byte == kReportHeader[0] || byte == kAckHeader[0]) {
      is_ack_ = (byte == kAckHeader[0]);
      buf_[len_++] = byte;
    }
    return false;
  }
  if (len_ < 4) {
    const uint8_t* header = is_ack_ ? kAckHeader : kReportHeader;
    buf_[len_++] = byte;
    if (byte == header[len_ - 1]) return false;
    return resync(out);
  }
  buf_[len_++] = byte;
  if (len_ < kPrefixLen) return false;
  uint16_t data_len = le16(buf_ + 4);
  if (data_len > sizeof(buf_) - kPrefixLen - kFooterLen) return resync(out);
  uint8_t total = kPrefixLen + data_len + kFooterLen;
  if (len_ < total) return false;
  const uint8_t* footer = is_ack_ ? kAckFooter : kReportFooter;
  for (uint8_t i = 0; i < kFooterLen; i++) {
    if (buf_[total - kFooterLen + i] != footer[i]) return resync(out);
  }
  const uint8_t* d = buf_ + kPrefixLen;
  if (is_ack_) {
    if (data_len < kMinAckDataLen) return resync(out);
    len_ = 0;
    out.kind = Frame::Kind::Ack;
    out.ack_command = le16(d);
    out.ack_ok = le16(d + 2) == 0;
    return true;
  }
  if (data_len < kMinReportDataLen || d[1] != kHeadMarker || d[data_len - 2] != kTailMarker) {
    return resync(out);
  }
  len_ = 0;
  out.kind = Frame::Kind::Report;
  out.report = decode_report(d);
  return true;
}

// Drops the first buffered byte and replays the rest, so a valid header that
// began inside the discarded bytes is still found.
bool FrameParser::resync(Frame& out) {
  uint8_t pending[sizeof(buf_)];
  uint8_t n = len_ - 1;
  for (uint8_t i = 0; i < n; i++) pending[i] = buf_[i + 1];
  len_ = 0;
  bool completed = false;
  for (uint8_t i = 0; i < n; i++) {
    if (feed(pending[i], out)) completed = true;
  }
  return completed;
}

size_t encode_command(uint16_t command, const uint8_t* value, size_t value_len, uint8_t* out) {
  size_t n = 0;
  for (uint8_t b : kAckHeader) out[n++] = b;
  uint16_t len = static_cast<uint16_t>(2 + value_len);
  out[n++] = len & 0xFF;
  out[n++] = len >> 8;
  out[n++] = command & 0xFF;
  out[n++] = command >> 8;
  for (size_t i = 0; i < value_len; i++) out[n++] = value[i];
  for (uint8_t b : kAckFooter) out[n++] = b;
  return n;
}

void SensorDetection::start(uint32_t now_ms) { start_ms_ = now_ms; }
void SensorDetection::on_frame(uint32_t now_ms) {
  if (status(now_ms) == Status::Probing) got_frame_ = true;
}
SensorDetection::Status SensorDetection::status(uint32_t now_ms) const {
  if (got_frame_) return Status::Sensor;
  if (now_ms - start_ms_ >= timeout_ms_) return Status::NoSensor;
  return Status::Probing;
}

Presence PresenceTracker::on_report(const Report& report) {
  bool present = report.target != TargetState::None;
  Presence result = {present, present && !present_, report};
  present_ = present;
  return result;
}

}  // namespace ld2410
}  // namespace sls
