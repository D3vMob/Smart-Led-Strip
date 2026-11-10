#pragma once
#include <cstdint>

namespace sls {

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
  enum class Kind : uint8_t { Open, Close, SetLevel, SetHueSat, SetMotionEnabled };
  Kind kind;
  uint8_t level_pct;
  uint16_t hue_deg;
  uint8_t saturation;
  bool motion_enabled;

  static Command open();
  static Command close();
  static Command set_level(uint8_t pct);
  static Command set_hue_sat(uint16_t hue_deg, uint8_t saturation);
  static Command set_motion_enabled(bool on);
};

GroupState default_group_state();
GroupState apply(GroupState state, const Command& command);

}  // namespace sls
