// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#pragma once

#include <Arduino.h>

#include "ld2410.h"

// LD2410C presence sensor on a hardware UART. At begin() it probes for the
// sensor with the read-firmware-version command; any valid frame within the
// detection timeout makes this Unit a Sensor unit for the rest of the session.
class Ld2410Uart {
 public:
  Ld2410Uart(HardwareSerial& serial, int8_t rx_pin, int8_t tx_pin, uint32_t detect_timeout_ms);
  void begin(uint32_t now_ms);

  // Reads the bytes waiting on the UART. Returns true and fills `out` when a
  // report frame arrived; `out` holds the latest one.
  bool poll(uint32_t now_ms, sls::ld2410::Presence& out);

  sls::ld2410::SensorDetection::Status status(uint32_t now_ms) const;

 private:
  void send_command(uint16_t command, const uint8_t* value, size_t value_len);

  HardwareSerial& serial_;
  int8_t rx_pin_;
  int8_t tx_pin_;
  sls::ld2410::FrameParser parser_;
  sls::ld2410::SensorDetection detection_;
  sls::ld2410::PresenceTracker presence_;
};
