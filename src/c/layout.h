#pragma once

#include <pebble.h>

// Screen geometry and type, per display. Everything is drawn on a grid of
// PX-sized "art pixels" so the scene matches the 2x / 3x cat sprites.
//
// Text boxes are placed by their top edge; system fonts leave a few pixels of
// headroom above the glyphs, so the digits sit lower than the box suggests.

#define SCREEN_W PBL_DISPLAY_WIDTH
#define SCREEN_H PBL_DISPLAY_HEIGHT

#if PBL_DISPLAY_HEIGHT >= 228
// Pebble Time 2 (emery): 200x228, sprites at 3x.
#define PX 3
#define FONT_TIME FONT_KEY_LECO_42_NUMBERS
#define FONT_STATUS FONT_KEY_GOTHIC_18_BOLD
#define FONT_STEPS FONT_KEY_GOTHIC_28_BOLD
#define FONT_GOAL FONT_KEY_GOTHIC_18
#define FONT_BUBBLE FONT_KEY_GOTHIC_18_BOLD
#define STATUS_Y -1
#define STATUS_H 22
#define STATUS_ICON_Y 6
#define TIME_Y 12
#define TIME_H 50
#define GROUND_Y 152
#define PANEL_Y 176
#define PANEL_PAD 8
#define STEPS_Y (PANEL_Y - 4)
#define STEPS_SMALL_DY 11
#define ROW1_W (SCREEN_W - 2 * PANEL_PAD)
#define LIVES_Y (PANEL_Y + 6)
#define BAR_Y (PANEL_Y + 32)
#define BAR_H 12
#define GOAL_Y (BAR_Y - 6)
#elif defined(PBL_ROUND)
// Pebble Time Round (chalk): 180x180 circle, sprites at 2x.
#define PX 2
#define FONT_TIME FONT_KEY_LECO_38_BOLD_NUMBERS
#define FONT_STATUS FONT_KEY_GOTHIC_14_BOLD
#define FONT_STEPS FONT_KEY_GOTHIC_18_BOLD
#define FONT_GOAL FONT_KEY_GOTHIC_14
#define FONT_BUBBLE FONT_KEY_GOTHIC_14_BOLD
#define STATUS_Y 13
#define STATUS_H 18
#define STATUS_ICON_Y (STATUS_Y + 5)
#define TIME_Y 22
#define TIME_H 44
#define GROUND_Y 122
#define PANEL_Y 140
// The circle is only this wide at the bar's row.
#define PANEL_ROW_W 86
#define PANEL_PAD ((SCREEN_W - PANEL_ROW_W) / 2)
#define STEPS_Y (PANEL_Y - 3)
#define STEPS_SMALL_DY 4
// The circle is about this wide at the steps row.
#define ROW1_W 118
#define LIVES_Y (PANEL_Y + 3)
#define BAR_Y (PANEL_Y + 21)
#define BAR_H 8
#define GOAL_Y (BAR_Y - 5)
#else
// Rectangular 144x168 (basalt, diorite, flint), sprites at 2x.
#define PX 2
#define FONT_TIME FONT_KEY_LECO_38_BOLD_NUMBERS
#define FONT_STATUS FONT_KEY_GOTHIC_14_BOLD
#define FONT_STEPS FONT_KEY_GOTHIC_24_BOLD
#define FONT_GOAL FONT_KEY_GOTHIC_14
#define FONT_BUBBLE FONT_KEY_GOTHIC_14_BOLD
#define STATUS_Y -2
#define STATUS_H 18
#define STATUS_ICON_Y 3
#define TIME_Y 8
#define TIME_H 44
#define GROUND_Y 110
#define PANEL_Y 128
#define PANEL_PAD 6
#define STEPS_Y (PANEL_Y - 4)
#define STEPS_SMALL_DY 10
#define ROW1_W (SCREEN_W - 2 * PANEL_PAD)
#define LIVES_Y (PANEL_Y + 5)
#define BAR_Y (PANEL_Y + 26)
#define BAR_H 8
#define GOAL_Y (BAR_Y - 5)
#endif

// Lowest point the clock's digits reach; speech bubbles stay below it.
#define CLOCK_BOTTOM (TIME_Y + TIME_H)

// Cats stand on two lanes of grass; the back lane is drawn first.
#define LANE_FRONT_Y (PANEL_Y - 2 * PX)
#define LANE_BACK_Y (PANEL_Y - 5 * PX)
// How close cats may walk to the screen edges.
#define CAT_MARGIN PBL_IF_ROUND_ELSE(12, 2)
