#include <Arduino.h>

#include "blink.h"
#include "config.h"
#include "group_state.h"
#include "save_scheduler.h"
#include "state_store.h"
#include "strip_output.h"

namespace {

StripOutput strip(LED_COUNT, LED_DATA_PIN, LED_PIXEL_TYPE);
sls::GroupState group_state = sls::default_group_state();
sls::SaveScheduler scheduler(group_state, PERSIST_SETTLE_MS);
sls::BlinkClock blink_clock;

}  // namespace

void setup() {
  strip.begin();
  // The Strip comes back the way it was left; a fresh Unit starts in the defaults (Closed).
  group_state = load_group_state();
  scheduler = sls::SaveScheduler(group_state, PERSIST_SETTLE_MS);
}

void loop() {
  scheduler.on_change(group_state, millis());
  if (scheduler.due(millis())) {
    save_group_state(group_state);
  }
  const uint32_t phase = blink_clock.phase_ms(millis(), sls::blink_period_ms(group_state.look.blink_speed_pct));
  const bool lit = sls::blink_lit(group_state.look, group_state.open, phase);
  strip.render(group_state.look, lit);
  // Short enough that the 50 ms minimum blink period still shows (25 ms on, 25 ms off).
  delay(10);
}
