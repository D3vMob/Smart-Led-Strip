#include "group_state.h"

#include "hue_picker.h"

namespace sls {

namespace {

// Below this level the Strip would look off while Open, so dimming never goes lower.
constexpr uint8_t kMinOpenLevelPct = 10;
constexpr uint8_t kMaxLevelPct = 100;
constexpr uint16_t kHueDegrees = 360;
constexpr uint8_t kFullSaturation = 255;

uint8_t clamp_open_level(uint8_t pct) {
  if (pct < kMinOpenLevelPct) return kMinOpenLevelPct;
  if (pct > kMaxLevelPct) return kMaxLevelPct;
  return pct;
}

}  // namespace

Command Command::open() { return Command{Kind::Open, 0, 0, 0, 0}; }
Command Command::close() { return Command{Kind::Close, 0, 0, 0, 0}; }
Command Command::set_level(uint8_t pct) { return Command{Kind::SetLevel, pct, 0, 0, 0}; }
Command Command::set_hue_sat(uint16_t hue_deg, uint8_t saturation) {
  return Command{Kind::SetHueSat, 0, hue_deg, saturation, 0};
}
Command Command::random_color(uint32_t random_value) {
  return Command{Kind::RandomColor, 0, 0, 0, random_value};
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
    case Command::Kind::RandomColor:
      // A ColorTemp Look has no hue to avoid, so the random value alone picks it.
      state.look.hue_deg = state.look.color_mode == ColorMode::ColorTemp
                               ? static_cast<uint16_t>(command.random_value % kHueDegrees)
                               : pick_hue(state.look.hue_deg, command.random_value);
      state.look.color_mode = ColorMode::HueSat;
      state.look.saturation = kFullSaturation;
      state.open = true;
      break;
  }
  return state;
}

}  // namespace sls
