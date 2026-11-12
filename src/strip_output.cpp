// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 adesbiens

#include "strip_output.h"

#include "frame_renderer.h"

StripOutput::StripOutput(uint16_t led_count, int16_t data_pin, neoPixelType pixel_type,
                         uint16_t max_milliamps)
    : pixels_(led_count, data_pin, pixel_type), led_count_(led_count), max_milliamps_(max_milliamps) {}

void StripOutput::begin() {
  pixels_.begin();
  pixels_.clear();
  pixels_.show();
}

void StripOutput::render(const sls::Look& look, bool lit) {
  const sls::Frame frame = sls::render(look, lit, led_count_, max_milliamps_);
  pixels_.fill(pixels_.Color(frame.r, frame.g, frame.b));
  pixels_.show();
}
