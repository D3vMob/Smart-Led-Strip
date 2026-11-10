#include "persisted_state.h"

namespace sls {

namespace {

constexpr uint8_t kFormatVersion = 1;

// Byte offsets inside the blob.
constexpr size_t kVersionAt = 0;
constexpr size_t kColorModeAt = 1;
constexpr size_t kHueAt = 2;  // uint16, little-endian
constexpr size_t kSaturationAt = 4;
constexpr size_t kColorTempAt = 5;  // uint16, little-endian
constexpr size_t kBrightnessAt = 7;
constexpr size_t kBlinkOnAt = 8;
constexpr size_t kBlinkSpeedAt = 9;
constexpr size_t kOpenAt = 10;
constexpr size_t kMotionEnabledAt = 11;
constexpr size_t kChecksumAt = 12;  // uint16, little-endian, over bytes before it
static_assert(kChecksumAt + 2 == kGroupStateBlobSize, "blob layout and size disagree");

constexpr uint8_t kMinBrightnessPct = 10;
constexpr uint8_t kMaxPct = 100;
constexpr uint16_t kHueDegrees = 360;

void put_u16(uint8_t* out, uint16_t value) {
  out[0] = static_cast<uint8_t>(value & 0xFF);
  out[1] = static_cast<uint8_t>(value >> 8);
}

uint16_t get_u16(const uint8_t* in) {
  return static_cast<uint16_t>(in[0] | (in[1] << 8));
}

// Fletcher-16 over the payload bytes.
uint16_t checksum(const uint8_t* data, size_t length) {
  uint16_t sum1 = 0;
  uint16_t sum2 = 0;
  for (size_t i = 0; i < length; ++i) {
    sum1 = (sum1 + data[i]) % 255;
    sum2 = (sum2 + sum1) % 255;
  }
  return static_cast<uint16_t>((sum2 << 8) | sum1);
}

bool is_flag(uint8_t byte) { return byte <= 1; }

// Checks the raw field bytes of a blob whose version and checksum are valid.
// Saturation and color temperature accept any value.
bool is_plausible(const uint8_t* b) {
  const uint8_t brightness = b[kBrightnessAt];
  return b[kColorModeAt] <= static_cast<uint8_t>(ColorMode::ColorTemp) &&
         get_u16(b + kHueAt) < kHueDegrees &&
         brightness >= kMinBrightnessPct && brightness <= kMaxPct &&
         b[kBlinkSpeedAt] <= kMaxPct &&
         is_flag(b[kBlinkOnAt]) && is_flag(b[kOpenAt]) && is_flag(b[kMotionEnabledAt]);
}

}  // namespace

GroupStateBlob encode_group_state(const GroupState& state) {
  GroupStateBlob blob{};
  uint8_t* b = blob.data();
  b[kVersionAt] = kFormatVersion;
  b[kColorModeAt] = static_cast<uint8_t>(state.look.color_mode);
  put_u16(b + kHueAt, state.look.hue_deg);
  b[kSaturationAt] = state.look.saturation;
  put_u16(b + kColorTempAt, state.look.color_temp_mireds);
  b[kBrightnessAt] = state.look.brightness_pct;
  b[kBlinkOnAt] = state.look.blink_on ? 1 : 0;
  b[kBlinkSpeedAt] = state.look.blink_speed_pct;
  b[kOpenAt] = state.open ? 1 : 0;
  b[kMotionEnabledAt] = state.motion_enabled ? 1 : 0;
  put_u16(b + kChecksumAt, checksum(b, kChecksumAt));
  return blob;
}

GroupState decode_group_state(const uint8_t* data, size_t length) {
  const bool readable = data != nullptr && length >= kGroupStateBlobSize &&
                        get_u16(data + kChecksumAt) == checksum(data, kChecksumAt) &&
                        data[kVersionAt] == kFormatVersion && is_plausible(data);
  if (!readable) {
    return default_group_state();
  }
  GroupState state{};
  state.look.color_mode = static_cast<ColorMode>(data[kColorModeAt]);
  state.look.hue_deg = get_u16(data + kHueAt);
  state.look.saturation = data[kSaturationAt];
  state.look.color_temp_mireds = get_u16(data + kColorTempAt);
  state.look.brightness_pct = data[kBrightnessAt];
  state.look.blink_on = data[kBlinkOnAt] != 0;
  state.look.blink_speed_pct = data[kBlinkSpeedAt];
  state.open = data[kOpenAt] != 0;
  state.motion_enabled = data[kMotionEnabledAt] != 0;
  return state;
}

}  // namespace sls
