#include <Arduino.h>

#include "config.h"
#include "group_state.h"
#include "save_scheduler.h"
#include "state_store.h"
#include "strip_output.h"

namespace {

StripOutput strip(LED_COUNT, LED_DATA_PIN, LED_PIXEL_TYPE);
sls::GroupState group_state = sls::default_group_state();
sls::SaveScheduler scheduler(group_state, PERSIST_SETTLE_MS);

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
  strip.render(group_state);
  delay(50);
}
