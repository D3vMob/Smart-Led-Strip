#include "group_state.h"

namespace sls {

namespace {

// Below this level the Strip would look off while Open, so dimming never goes lower.
constexpr uint8_t kMinOpenLevelPct = 10;
constexpr uint8_t kMaxLevelPct = 100;
constexpr uint16_t kHueDegrees = 360;
constexpr uint8_t kMinBlinkSpeedPct = 1;
constexpr uint8_t kMaxBlinkSpeedPct = 100;

uint8_t clamp_open_level(uint8_t pct) {
  if (pct < kMinOpenLevelPct) return kMinOpenLevelPct;
  if (pct > kMaxLevelPct) return kMaxLevelPct;
  return pct;
}

Command of_kind(Command::Kind kind) {
  Command command;
  command.kind = kind;
  return command;
}

}  // namespace

uint8_t clamp_blink_speed(uint8_t pct) {
  if (pct < kMinBlinkSpeedPct) return kMinBlinkSpeedPct;
  if (pct > kMaxBlinkSpeedPct) return kMaxBlinkSpeedPct;
  return pct;
}

Command Command::open() { return of_kind(Kind::Open); }
Command Command::close() { return of_kind(Kind::Close); }
Command Command::set_level(uint8_t pct) {
  Command command = of_kind(Kind::SetLevel);
  command.level_pct = pct;
  return command;
}
Command Command::set_hue_sat(uint16_t hue_deg, uint8_t saturation) {
  Command command = of_kind(Kind::SetHueSat);
  command.hue_deg = hue_deg;
  command.saturation = saturation;
  return command;
}
Command Command::set_blink(bool on) {
  Command command = of_kind(Kind::SetBlink);
  command.blink_on = on;
  return command;
}
Command Command::set_blink_speed(uint8_t pct) {
  Command command = of_kind(Kind::SetBlinkSpeed);
  command.blink_speed_pct = pct;
  return command;
}

GroupState default_group_state() {
  GroupState state{};
  state.look = Look{ColorMode::HueSat, 0, 0, 250, 100, false, 20};
  state.open = false;
  state.motion_enabled = false;
  return state;
}

GroupState apply(GroupState state, const Command& command) {
  switch (command.kind) {
    case Command::Kind::Open:
      state.open = true;
      break;
    case Command::Kind::Close:
      state.open = false;
      break;
    case Command::Kind::SetLevel:
      // Dimming to zero closes the Strip but keeps the Look's brightness for next time.
      if (command.level_pct == 0) {
        state.open = false;
        break;
      }
      state.look.brightness_pct = clamp_open_level(command.level_pct);
      state.open = true;
      break;
    case Command::Kind::SetHueSat:
      state.look.color_mode = ColorMode::HueSat;
      state.look.hue_deg = command.hue_deg % kHueDegrees;
      state.look.saturation = command.saturation;
      state.open = true;
      break;
    case Command::Kind::SetBlink:
      state.look.blink_on = command.blink_on;
      break;
    case Command::Kind::SetBlinkSpeed:
      state.look.blink_speed_pct = clamp_blink_speed(command.blink_speed_pct);
      break;
  }
  return state;
}

}  // namespace sls
