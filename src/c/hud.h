#pragma once

#include <pebble.h>
#include "game.h"
#include "scene.h"

// Date and battery along the top edge.
void hud_draw_status(GContext *ctx, const struct tm *now, BatteryChargeState battery,
                     const ScenePalette *palette);
void hud_draw_clock(GContext *ctx, const struct tm *now, const ScenePalette *palette);
// Steps, goal bar and lives along the bottom.
void hud_draw_panel(GContext *ctx, const GameState *state);
void hud_draw_battery(GContext *ctx, GPoint origin, BatteryChargeState battery);
GSize hud_battery_size(void);
