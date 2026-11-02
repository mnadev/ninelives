#include "hud.h"
#include "layout.h"
#include "pixel.h"

#define BATTERY_CELLS 5

static const char BATTERY[][9] = {
  "ooooooo.",
  "o.....oo",
  "o.....oo",
  "ooooooo.",
};

static const char CAT_HEAD[][8] = {
  "o.....o",
  "oo...oo",
  "ooooooo",
  "o.ooo.o",
  "ooooooo",
  ".ooooo.",
};

static const char CAT_HEAD_EMPTY[][8] = {
  "o.....o",
  "oo...oo",
  "o.ooo.o",
  "o.....o",
  "o.....o",
  ".ooooo.",
};

static const char TREND_UP[][6] = {
  "..o..",
  ".ooo.",
  "ooooo",
};

static const char TREND_DOWN[][6] = {
  "ooooo",
  ".ooo.",
  "..o..",
};

static void prv_uppercase(char *text) {
  for (; *text; text++) {
    if (*text >= 'a' && *text <= 'z') {
      *text -= 'a' - 'A';
    }
  }
}

// 12345 -> "12,345"
static void prv_format_count(char *buffer, size_t size, int32_t value) {
  char digits[12];
  snprintf(digits, sizeof(digits), "%ld", (long)(value < 0 ? 0 : value));
  const int length = strlen(digits);
  size_t out = 0;
  for (int i = 0; i < length && out + 1 < size; i++) {
    if (i > 0 && (length - i) % 3 == 0 && out + 2 < size) {
      buffer[out++] = ',';
    }
    buffer[out++] = digits[i];
  }
  buffer[out] = '\0';
}

static GSize prv_text_size(const char *text, GFont font) {
  return graphics_text_layout_get_content_size(text, font, GRect(0, 0, SCREEN_W, 64),
                                               GTextOverflowModeFill, GTextAlignmentLeft);
}

static void prv_draw_text(GContext *ctx, const char *text, GFont font, GRect box,
                          GTextAlignment align, GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font, box, GTextOverflowModeFill, align, NULL);
}

// White text with a hard drop shadow so it reads on any sky.
static void prv_draw_on_sky(GContext *ctx, const char *text, GFont font, GRect box,
                            GTextAlignment align, GColor shadow, int offset) {
#ifdef PBL_COLOR
  const GRect shadow_box = GRect(box.origin.x + offset, box.origin.y + offset,
                                 box.size.w, box.size.h);
  prv_draw_text(ctx, text, font, shadow_box, align, shadow);
#endif
  prv_draw_text(ctx, text, font, box, align, GColorWhite);
}

GSize hud_battery_size(void) {
  return pixel_size(PIXEL_ART(BATTERY), PX);
}

void hud_draw_battery(GContext *ctx, GPoint origin, BatteryChargeState battery) {
  const PixelInk inks[] = { { 'o', GColorWhite } };
  pixel_draw(ctx, PIXEL_ART(BATTERY), origin, PX, inks, ARRAY_LENGTH(inks));

  int cells = (battery.charge_percent + 19) / 20;
  cells = cells < 1 ? 1 : (cells > BATTERY_CELLS ? BATTERY_CELLS : cells);
  GColor fill = GColorWhite;
#ifdef PBL_COLOR
  if (battery.is_charging || battery.is_plugged) {
    fill = GColorMalachite;
  } else if (battery.charge_percent <= 20) {
    fill = GColorRed;
  }
#endif
  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, GRect(origin.x + PX, origin.y + PX, cells * PX, 2 * PX), 0, GCornerNone);
}

void hud_draw_status(GContext *ctx, const struct tm *now, BatteryChargeState battery,
                     const ScenePalette *palette) {
  char weekday[8];
  char month[8];
  char text[24];
  strftime(weekday, sizeof(weekday), "%a", now);
  strftime(month, sizeof(month), "%b", now);
  snprintf(text, sizeof(text), "%s %s %d", weekday, month, now->tm_mday);
  prv_uppercase(text);

  const GFont font = fonts_get_system_font(FONT_STATUS);
  const GSize battery_size = hud_battery_size();
#ifdef PBL_ROUND
  // Date and battery centred as one group.
  const GSize text_size = prv_text_size(text, font);
  const int x = (SCREEN_W - (text_size.w + 2 * PX + battery_size.w)) / 2;
  prv_draw_on_sky(ctx, text, font, GRect(x, STATUS_Y, text_size.w + 4, STATUS_H),
                  GTextAlignmentLeft, palette->text_shadow, 1);
  hud_draw_battery(ctx, GPoint(x + text_size.w + 2 * PX, STATUS_ICON_Y), battery);
#else
  prv_draw_on_sky(ctx, text, font,
                  GRect(PANEL_PAD, STATUS_Y, SCREEN_W - 2 * PANEL_PAD, STATUS_H),
                  GTextAlignmentLeft, palette->text_shadow, 1);
  hud_draw_battery(ctx, GPoint(SCREEN_W - PANEL_PAD - battery_size.w, STATUS_ICON_Y), battery);
#endif
}

void hud_draw_clock(GContext *ctx, const struct tm *now, const ScenePalette *palette) {
  static char text[8];
  const bool is_24h = clock_is_24h_style();
  strftime(text, sizeof(text), is_24h ? "%H:%M" : "%I:%M", now);
  const char *shown = (!is_24h && text[0] == '0') ? text + 1 : text;
  prv_draw_on_sky(ctx, shown, fonts_get_system_font(FONT_TIME),
                  GRect(0, TIME_Y, SCREEN_W, TIME_H), GTextAlignmentCenter,
                  palette->text_shadow, 2);
}

static GColor prv_bar_color(const GameState *state) {
#ifdef PBL_COLOR
  if (state->is_dead) {
    return GColorPictonBlue;
  }
  switch (state->pace_state) {
    case PET_STATE_LAZY:
      return GColorYellow;
    case PET_STATE_DISTRESSED:
      return GColorRed;
    default:
      return GColorMalachite;
  }
#else
  return GColorWhite;
#endif
}

static void prv_draw_bar(GContext *ctx, GRect bar, int32_t value, int32_t total, GColor color,
                         int32_t marker) {
  int fill_w = total > 0 ? (int)((int64_t)bar.size.w * value / total) : 0;
  fill_w = fill_w < 0 ? 0 : (fill_w > bar.size.w ? bar.size.w : fill_w);
  if (value > 0 && fill_w < PX) {
    fill_w = PX;
  }
  fill_w = fill_w / PX * PX;

#ifdef PBL_COLOR
  graphics_context_set_fill_color(ctx, GColorDarkGray);
  graphics_fill_rect(ctx, bar, 0, GCornerNone);
#else
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_rect(ctx, bar);
#endif
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, GRect(bar.origin.x, bar.origin.y, fill_w, bar.size.h), 0, GCornerNone);

  // The point past which today is safe, when that's short of the goal.
  if (marker > 0 && marker < total) {
    const int x = bar.origin.x + (int)((int64_t)bar.size.w * marker / total) / PX * PX;
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(x, bar.origin.y - PX, PX, bar.size.h + 2 * PX), 0, GCornerNone);
  }
}

static void prv_draw_lives(GContext *ctx, GPoint origin, int count) {
  const GSize size = pixel_size(PIXEL_ART(CAT_HEAD), PX);
  for (int i = 0; i < MAX_CATS; i++) {
    const bool alive = i < count;
    const PixelInk inks[] = {
      { 'o', alive ? GColorWhite : PBL_IF_COLOR_ELSE(GColorDarkGray, GColorWhite) },
    };
    PixelArt head = PIXEL_ART(CAT_HEAD_EMPTY);
    if (alive) {
      head = PIXEL_ART(CAT_HEAD);
    }
    pixel_draw(ctx, head, GPoint(origin.x + i * (size.w + PX), origin.y), PX, inks,
               ARRAY_LENGTH(inks));
  }
}

static int prv_lives_width(void) {
  return MAX_CATS * pixel_size(PIXEL_ART(CAT_HEAD), PX).w + (MAX_CATS - 1) * PX;
}

void hud_draw_panel(GContext *ctx, const GameState *state) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(0, PANEL_Y, SCREEN_W, SCREEN_H - PANEL_Y), 0, GCornerNone);

  // Dead cats show how far along the walk to bring one back is.
  const bool dead = state->is_dead;
  const int32_t value = dead ? state->resurrection_progress : state->steps;
  const int32_t total = dead ? state->resurrection_goal : state->step_goal;

  char steps_text[16];
  char goal_text[16];
  prv_format_count(steps_text, sizeof(steps_text), dead && value > total ? total : value);
  prv_format_count(goal_text, sizeof(goal_text), total);

  const GFont goal_font = fonts_get_system_font(FONT_GOAL);
  const GFont label_font = fonts_get_system_font(FONT_STATUS);
  const GSize goal_size = prv_text_size(goal_text, goal_font);
  const char *label = "REVIVE";
  const int badge_w = dead ? prv_text_size(label, label_font).w : prv_lives_width();

  // Row 1: today's steps, then the lives (or what reviving means). A huge
  // day drops to smaller type rather than running into them.
  const int gap = 3 * PX;
  GFont steps_font = fonts_get_system_font(FONT_STEPS);
  GSize steps_size = prv_text_size(steps_text, steps_font);
  int steps_y = STEPS_Y;
  if (steps_size.w + gap + badge_w > ROW1_W) {
    steps_font = label_font;
    steps_size = prv_text_size(steps_text, steps_font);
    steps_y = STEPS_Y + STEPS_SMALL_DY;
  }
#ifdef PBL_ROUND
  const int steps_x = (SCREEN_W - steps_size.w - gap - badge_w) / 2;
  const int badge_x = steps_x + steps_size.w + gap;
#else
  const int steps_x = PANEL_PAD;
  const int badge_x = SCREEN_W - PANEL_PAD - badge_w;
#endif
  prv_draw_text(ctx, steps_text, steps_font, GRect(steps_x, steps_y, steps_size.w + 2, 36),
                GTextAlignmentLeft, GColorWhite);
  if (dead) {
    prv_draw_text(ctx, label, label_font, GRect(badge_x, LIVES_Y - 4, badge_w + 2, 24),
                  GTextAlignmentLeft, PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorWhite));
  } else {
    prv_draw_lives(ctx, GPoint(badge_x, LIVES_Y), state->cat_count);
  }

  // Row 2: the bar, ending in the number it's heading for, with an arrow when
  // the smart goal moved since yesterday.
  const int trend = dead ? 0 : state->goal_trend;
  // Chosen with an if: the SDK's GCC 4.7 folds a ?: between two PIXEL_ART
  // literals into always using the second one.
  PixelArt arrow = PIXEL_ART(TREND_DOWN);
  if (trend > 0) {
    arrow = PIXEL_ART(TREND_UP);
  }
  const GSize arrow_size = pixel_size(arrow, PX);
  const int goal_gap = 2 * PX;
  const int arrow_w = trend ? arrow_size.w + PX : 0;
  const int bar_w = (SCREEN_W - 2 * PANEL_PAD - goal_size.w - goal_gap - arrow_w) / PX * PX;
  const int32_t marker = dead ? 0 : state->survival_steps;
  prv_draw_bar(ctx, GRect(PANEL_PAD, BAR_Y, bar_w, BAR_H), value, total, prv_bar_color(state),
               marker);
  const int goal_x = PANEL_PAD + bar_w + goal_gap;
  if (trend) {
    const PixelInk inks[] = {
      { 'o', trend > 0 ? PBL_IF_COLOR_ELSE(GColorMalachite, GColorWhite)
                       : PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite) },
    };
    pixel_draw(ctx, arrow, GPoint(goal_x, BAR_Y + (BAR_H - arrow_size.h) / 2 / PX * PX), PX,
               inks, ARRAY_LENGTH(inks));
  }
  prv_draw_text(ctx, goal_text, goal_font, GRect(goal_x + arrow_w, GOAL_Y, goal_size.w + 2, 24),
                GTextAlignmentLeft, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
}
