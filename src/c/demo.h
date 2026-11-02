#pragma once

// Canned scenes for screenshots, compiled in only when building with
// `NINELIVES_DEMO=<first scene> pebble build`. A tap moves to the next scene.

#ifdef NINELIVES_DEMO

#include <pebble.h>
#include "game.h"

void demo_init(void);
void demo_next(void);
const GameState *demo_state(void);
const GameSettings *demo_settings(void);
struct tm demo_now(void);
BatteryChargeState demo_battery(void);
bool demo_bubble(void);

#endif
