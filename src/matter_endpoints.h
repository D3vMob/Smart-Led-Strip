#pragma once

#include "group_state.h"

// The device's five Matter endpoints (Strip, Blink, Blink speed, Motion,
// Random color), driving a Group state. Writes from Alexa/Google go through the
// endpoint mapping into the reducer; sync() then pushes back whatever the mapping
// says for the current state, so clamped values and changes from elsewhere
// (motion, boot restore) show in the app.
namespace matter_endpoints {

// Registers the endpoints with the initial Group state, starts Matter and,
// when the Unit is not commissioned yet, prints the QR code URL and manual
// pairing code on serial (repeated every 5 s by sync() until commissioned).
void begin(const sls::GroupState& initial);

// Whether the Unit has been commissioned into a Matter fabric.
bool commissioned();

// Applies the writes received since the last call to the Group state, then
// reports every endpoint's attributes from it. Call from loop(): Matter
// callbacks run on another task and only queue their commands. Cheap when
// nothing changed: unchanged attributes are not re-sent.
void sync(sls::GroupState& state);

}  // namespace matter_endpoints
