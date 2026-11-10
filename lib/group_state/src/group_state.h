#pragma once
#include <cstdint>

namespace sls {

// Below this level the Strip would look off while Open, so dimming never goes lower.
constexpr uint8_t kMinOpenLevelPct = 10;
constexpr uint8_t kMaxLevelPct = 100;

enum class ColorMode : uint8_t { HueSat, ColorTemp };

struct Look {
  ColorMode color_mode;
  uint16_t hue_deg;
  uint8_t saturation;
  uint16_t color_temp_mireds;
  uint8_t brightness_pct;
  bool blink_on;
  uint8_t blink_speed_pct;
};

struct GroupState {
  Look look;
  bool open;
  bool motion_enabled;
};

struct Command {
  enum class Kind : uint8_t { Open, Close, SetLevel, SetHueSat, SetBlink, SetBlinkSpeed, RandomColor, SetColorTemp };
  Kind kind = Kind::Open;
  uint8_t level_pct = 0;
  uint16_t hue_deg = 0;
  uint8_t saturation = 0;
  bool blink_on = false;
  uint8_t blink_speed_pct = 0;
  uint32_t random_value = 0;
  uint16_t color_temp_mireds = 0;

  static Command open();
  static Command close();
  static Command set_level(uint8_t pct);
  static Command set_hue_sat(uint16_t hue_deg, uint8_t saturation);
  static Command set_blink(bool on);
  static Command set_blink_speed(uint8_t pct);
  static Command random_color(uint32_t random_value);
  static Command set_color_temp(uint16_t mireds);
};

// Blink speed percentage clamped to 1-100.
uint8_t clamp_blink_speed(uint8_t pct);

GroupState default_group_state();
GroupState apply(GroupState state, const Command& command);

}  // namespace sls
