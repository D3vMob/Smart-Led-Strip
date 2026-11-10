#include <Arduino.h>

#include "config.h"
#include "group_state.h"
#include "strip_output.h"

namespace {

StripOutput strip(LED_COUNT, LED_DATA_PIN, LED_PIXEL_TYPE);
sls::GroupState group_state = sls::default_group_state();

}  // namespace

void setup() {
  strip.begin();
  // Until Matter drives it, open the Strip on boot so the bench Unit shows its default Look.
  group_state = sls::apply(group_state, sls::Command::open());
}

void loop() {
  strip.render(group_state);
  delay(50);
}
