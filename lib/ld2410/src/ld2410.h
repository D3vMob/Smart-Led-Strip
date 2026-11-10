#pragma once

#include <stddef.h>
#include <stdint.h>

namespace sls {
namespace ld2410 {

enum class TargetState : uint8_t { None = 0, Moving = 1, Still = 2, MovingAndStill = 3 };

struct Report {
  TargetState target;
  uint16_t moving_distance_cm;
  uint8_t moving_energy;
  uint16_t still_distance_cm;
  uint8_t still_energy;
  uint16_t detection_distance_cm;
};

struct Frame {
  enum class Kind : uint8_t { Report, Ack };
  Kind kind;
  Report report;
  uint16_t ack_command;
  bool ack_ok;
};

class FrameParser {
 public:
  // Returns true when this byte completes a valid frame, filling `out`.
  bool feed(uint8_t byte, Frame& out);

 private:
  bool resync(Frame& out);
  uint8_t buf_[32] = {};
  uint8_t len_ = 0;
  bool is_ack_ = false;
};

const uint16_t kCmdEnableConfig = 0x00FF;
const uint16_t kCmdEndConfig = 0x00FE;
const uint16_t kCmdReadFirmwareVersion = 0x00A0;

// Builds a command frame into `out`; returns the number of bytes written.
size_t encode_command(uint16_t command, const uint8_t* value, size_t value_len, uint8_t* out);

// Decides whether a sensor is attached: the first frame within the timeout
// after start() means Sensor, otherwise NoSensor. The result is held.
class SensorDetection {
 public:
  enum class Status : uint8_t { Probing, Sensor, NoSensor };

  explicit SensorDetection(uint32_t timeout_ms) : timeout_ms_(timeout_ms) {}
  void start(uint32_t now_ms);
  void on_frame(uint32_t now_ms);
  Status status(uint32_t now_ms) const;

 private:
  uint32_t timeout_ms_;
  uint32_t start_ms_ = 0;
  bool got_frame_ = false;
};

struct Presence {
  bool present;
  bool onset;  // true only on the report where presence begins
  Report report;
};

class PresenceTracker {
 public:
  Presence on_report(const Report& report);

 private:
  bool present_ = false;
};

}  // namespace ld2410
}  // namespace sls
