#include <Arduino.h>

#include "blink.h"
#include "config.h"
#include "group_state.h"
#include "strip_output.h"

namespace {

StripOutput strip(LED_COUNT, LED_DATA_PIN, LED_PIXEL_TYPE);
sls::GroupState group_state = sls::default_group_state();
sls::BlinkClock blink_clock;

}  // namespace

void setup() {
  strip.begin();
  // Until Matter drives it, open the Strip on boot so the bench Unit shows its default Look.
  group_state = sls::apply(group_state, sls::Command::open());
}

void loop() {
  const uint32_t phase = blink_clock.phase_ms(millis(), sls::blink_period_ms(group_state.look.blink_speed_pct));
  const bool lit = sls::blink_lit(group_state.look, group_state.open, phase);
  strip.render(group_state.look, lit);
  // Short enough that the 50 ms minimum blink period still shows (25 ms on, 25 ms off).
  delay(10);
}
