#include "ld2410_uart.h"

namespace {
const uint32_t kBaud = 256000;
const size_t kRxBufferSize = 1024;
}  // namespace

Ld2410Uart::Ld2410Uart(HardwareSerial& serial, int8_t rx_pin, int8_t tx_pin,
                       uint32_t detect_timeout_ms)
    : serial_(serial), rx_pin_(rx_pin), tx_pin_(tx_pin), detection_(detect_timeout_ms) {}

void Ld2410Uart::begin(uint32_t now_ms) {
  serial_.setRxBufferSize(kRxBufferSize);
  serial_.begin(kBaud, SERIAL_8N1, rx_pin_, tx_pin_);
  detection_.start(now_ms);
  // The firmware version can only be read in configuration mode.
  const uint8_t enable_value[2] = {0x01, 0x00};
  send_command(sls::ld2410::kCmdEnableConfig, enable_value, sizeof(enable_value));
  send_command(sls::ld2410::kCmdReadFirmwareVersion, nullptr, 0);
  send_command(sls::ld2410::kCmdEndConfig, nullptr, 0);
}

bool Ld2410Uart::poll(uint32_t now_ms, sls::ld2410::Presence& out) {
  bool got_report = false;
  sls::ld2410::Frame frame;
  while (serial_.available() > 0) {
    if (!parser_.feed(static_cast<uint8_t>(serial_.read()), frame)) continue;
    detection_.on_frame(now_ms);
    if (frame.kind != sls::ld2410::Frame::Kind::Report) continue;
    sls::ld2410::Presence presence = presence_.on_report(frame.report);
    bool onset = presence.onset || (got_report && out.onset);
    out = presence;
    out.onset = onset;  // keep an onset seen earlier in this same poll
    got_report = true;
  }
  return got_report;
}

sls::ld2410::SensorDetection::Status Ld2410Uart::status(uint32_t now_ms) const {
  return detection_.status(now_ms);
}

void Ld2410Uart::send_command(uint16_t command, const uint8_t* value, size_t value_len) {
  uint8_t frame[16];
  size_t n = sls::ld2410::encode_command(command, value, value_len, frame);
  serial_.write(frame, n);
  serial_.flush();
}
