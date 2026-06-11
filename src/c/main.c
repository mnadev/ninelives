#include <pebble.h>
#include "game.h"
#include "sprites.h"

#define ANIM_INTERVAL_MS 200
#define ANIM_DURATION_MS 4000

#if PBL_DISPLAY_HEIGHT >= 228
#define FLICK_JUMP_HEIGHT 10
#define FONT_TIME FONT_KEY_LECO_42_NUMBERS
#define FONT_DATE FONT_KEY_GOTHIC_18
#define FONT_STEPS FONT_KEY_GOTHIC_24_BOLD
#define FONT_SPEECH FONT_KEY_GOTHIC_18
#define TIME_LAYER_H 46
#define TIME_LAYER_Y -2
#define DATE_LAYER_Y 42
#define DATE_LAYER_H 22
#define STEPS_LAYER_X 6
#define STEPS_LAYER_Y_OFFSET 9
#define STEPS_LAYER_W 80
#define STEPS_LAYER_H 30
#define SPEECH_OFFSET_X 32
#define SPEECH_OFFSET_Y -22
#define SPEECH_BUBBLE_W 44
#define SPEECH_BUBBLE_H 20
#define SPEECH_BUBBLE_R 5
#define SPEECH_TEXT_PAD 2
#define BATTERY_ICON_W 12
#define BATTERY_ICON_H 8
#define BATTERY_TIP_W 3
#define BATTERY_TIP_H 5
#define BATTERY_FILL_W 6
#define BATTERY_FILL_H 3
#define BATTERY_OFFSET_X 8
#define BATTERY_OFFSET_Y 3
#define BUTTERFLY_OFFSET_X 6
#define BUTTERFLY_OFFSET_Y 8
#define GHOST_FLOAT_OFFSET 8
#define GHOST_FLOAT_RANGE 3
#define MOON_X_OFFSET 22
#define MOON_Y 14
#define MOON_RADIUS 6
#else
#define FLICK_JUMP_HEIGHT 8
#define FONT_TIME FONT_KEY_LECO_36_BOLD_NUMBERS
#define FONT_DATE FONT_KEY_GOTHIC_14
#define FONT_STEPS FONT_KEY_GOTHIC_18_BOLD
#define FONT_SPEECH FONT_KEY_GOTHIC_14
#define TIME_LAYER_H 40
#define TIME_LAYER_Y -2
#define DATE_LAYER_Y 34
#define DATE_LAYER_H 18
#define STEPS_LAYER_X 4
#define STEPS_LAYER_Y_OFFSET 7
#define STEPS_LAYER_W 66
#define STEPS_LAYER_H 24
#define SPEECH_OFFSET_X 26
#define SPEECH_OFFSET_Y -18
#define SPEECH_BUBBLE_W 36
#define SPEECH_BUBBLE_H 16
#define SPEECH_BUBBLE_R 4
#define SPEECH_TEXT_PAD 1
#define BATTERY_ICON_W 10
#define BATTERY_ICON_H 6
#define BATTERY_TIP_W 2
#define BATTERY_TIP_H 4
#define BATTERY_FILL_W 5
#define BATTERY_FILL_H 2
#define BATTERY_OFFSET_X 6
#define BATTERY_OFFSET_Y 2
#define BUTTERFLY_OFFSET_X 4
#define BUTTERFLY_OFFSET_Y 6
#define GHOST_FLOAT_OFFSET 6
#define GHOST_FLOAT_RANGE 2
#define MOON_X_OFFSET 18
#define MOON_Y 12
#define MOON_RADIUS 5
#endif

static Window *s_window;
static Layer *s_stage_layer;
static Layer *s_hud_layer;
static TextLayer *s_time_layer;
static TextLayer *s_date_layer;
static TextLayer *s_steps_layer;

static AppTimer *s_anim_timer;
static uint8_t s_anim_frame;
static uint32_t s_anim_elapsed_ms;
static bool s_anim_active;
static bool s_flick_interrupt;
static int8_t s_jump_offset;
static uint8_t s_ghost_frame;
static bool s_battery_flash;

static GBitmap *s_fish_skeleton;
static GBitmap *s_fish_meat;
static GBitmap *s_ghost_bitmap;
static GBitmap *s_butterfly_bitmap;

static void prv_request_redraw(void) {
  layer_mark_dirty(s_stage_layer);
  layer_mark_dirty(s_hud_layer);
}

static void prv_stop_animation(void) {
  if (s_anim_timer) {
    app_timer_cancel(s_anim_timer);
    s_anim_timer = NULL;
  }
  s_anim_active = false;
}

static void prv_anim_timer_callback(void *data);

static void prv_start_animation(void) {
  prv_stop_animation();
  s_anim_frame = 0;
  s_anim_elapsed_ms = 0;
  s_anim_active = true;
  const GameState *state = game_get_state();
  sprites_load_frame(state->pet_state, 0);
  s_anim_timer = app_timer_register(ANIM_INTERVAL_MS, prv_anim_timer_callback, NULL);
  prv_request_redraw();
}

static void prv_anim_timer_callback(void *data) {
  const GameState *state = game_get_state();
  uint8_t frame_count = sprites_get_frame_count(state->pet_state);

  s_anim_frame = (s_anim_frame + 1) % frame_count;
  sprites_load_frame(state->pet_state, s_anim_frame);

  if (state->pet_state == PET_STATE_DEAD) {
    s_ghost_frame = (s_ghost_frame + 1) % 2;
    if (s_ghost_bitmap) {
      gbitmap_destroy(s_ghost_bitmap);
    }
    s_ghost_bitmap = gbitmap_create_with_resource(
        s_ghost_frame == 0 ? RESOURCE_ID_IMAGE_GHOST_0 : RESOURCE_ID_IMAGE_GHOST_1);
  }

  if (state->pet_state == PET_STATE_ENERGETIC && !s_flick_interrupt) {
    if (s_butterfly_bitmap) {
      gbitmap_destroy(s_butterfly_bitmap);
    }
    uint8_t bframe = s_anim_frame % 2;
    s_butterfly_bitmap = gbitmap_create_with_resource(
        bframe == 0 ? RESOURCE_ID_IMAGE_BUTTERFLY_0 : RESOURCE_ID_IMAGE_BUTTERFLY_1);
  }

  if (state->pet_state == PET_STATE_LOW_BATTERY) {
    s_battery_flash = !s_battery_flash;
  }

  s_anim_elapsed_ms += ANIM_INTERVAL_MS;
  prv_request_redraw();

  if (s_anim_elapsed_ms >= ANIM_DURATION_MS) {
    prv_stop_animation();
    sprites_load_frame(state->pet_state, 0);
    prv_request_redraw();
    return;
  }

  s_anim_timer = app_timer_register(ANIM_INTERVAL_MS, prv_anim_timer_callback, NULL);
}

static void prv_flick_reset_cb(void *data) {
  s_flick_interrupt = false;
  s_jump_offset = 0;
  prv_request_redraw();
}

static void prv_flick_jump_cb(void *data) {
  int step = (int)(intptr_t)data;
  if (step == 0) {
    s_jump_offset = -FLICK_JUMP_HEIGHT;
  } else if (step == 1) {
    s_jump_offset = -FLICK_JUMP_HEIGHT / 2;
  } else {
    s_jump_offset = 0;
  }
  prv_request_redraw();
}

static void prv_draw_sky(GContext *ctx, SkyPhase phase) {
  GRect bounds = layer_get_bounds(s_stage_layer);
  GColor top;
  GColor bottom;

  switch (phase) {
    case SKY_DAY:
      top = GColorVividCerulean;
      bottom = GColorCeleste;
      break;
    case SKY_SUNSET:
      top = GColorOrange;
      bottom = GColorChromeYellow;
      break;
    default:
      top = GColorOxfordBlue;
      bottom = GColorImperialPurple;
      break;
  }

  graphics_context_set_fill_color(ctx, top);
  graphics_fill_rect(ctx, GRect(0, 0, bounds.size.w, bounds.size.h / 2), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, bottom);
  graphics_fill_rect(ctx, GRect(0, bounds.size.h / 2, bounds.size.w, bounds.size.h / 2), 0, GCornerNone);

  if (phase == SKY_NIGHT) {
    graphics_context_set_fill_color(ctx, GColorWhite);
    for (int i = 0; i < 8; i++) {
      int x = 12 + (i * 17) % bounds.size.w;
      int y = 6 + (i * 9) % (bounds.size.h / 2);
      graphics_fill_circle(ctx, GPoint(x, y), 1);
    }
    graphics_fill_circle(ctx, GPoint(bounds.size.w - MOON_X_OFFSET, MOON_Y), MOON_RADIUS);
  }
}

static void prv_draw_ground(GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorJaegerGreen);
  graphics_fill_rect(ctx, GRect(0, GROUND_OFFSET, SCREEN_W, STAGE_H - GROUND_OFFSET), 0, GCornerNone);
  graphics_context_set_stroke_color(ctx, GColorDarkGreen);
  graphics_draw_line(ctx, GPoint(0, GROUND_OFFSET), GPoint(SCREEN_W, GROUND_OFFSET));
}

static void prv_draw_speech_bubble(GContext *ctx, int cat_x, int cat_y) {
  int bx = cat_x + SPEECH_OFFSET_X;
  int by = cat_y + SPEECH_OFFSET_Y;
  if (by < 2) {
    by = 2;
  }
  if (bx + SPEECH_BUBBLE_W + 2 > SCREEN_W) {
    bx = SCREEN_W - SPEECH_BUBBLE_W - 2;
  }
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(bx, by, SPEECH_BUBBLE_W, SPEECH_BUBBLE_H), SPEECH_BUBBLE_R, GCornersAll);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_draw_round_rect(ctx, GRect(bx, by, SPEECH_BUBBLE_W, SPEECH_BUBBLE_H), SPEECH_BUBBLE_R);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, "meow!", fonts_get_system_font(FONT_SPEECH),
                     GRect(bx + SPEECH_TEXT_PAD, by - 2, SPEECH_BUBBLE_W - SPEECH_TEXT_PAD * 2, SPEECH_BUBBLE_H),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void prv_draw_battery_icon(GContext *ctx, int x, int y, bool flash_on) {
  if (!flash_on) {
    return;
  }
  graphics_context_set_fill_color(ctx, GColorRed);
  graphics_fill_rect(ctx, GRect(x, y, BATTERY_ICON_W, BATTERY_ICON_H), 2, GCornersAll);
  graphics_fill_rect(ctx, GRect(x + BATTERY_ICON_W, y + 1, BATTERY_TIP_W, BATTERY_TIP_H), 1, GCornersAll);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(x + 2, y + 2, BATTERY_FILL_W, BATTERY_FILL_H), 0, GCornerNone);
}

static void prv_stage_update_proc(Layer *layer, GContext *ctx) {
  const GameState *state = game_get_state();
  GBitmap *cat = sprites_get_current();

  prv_draw_sky(ctx, state->sky_phase);
  prv_draw_ground(ctx);

  // Bitmaps have transparent backgrounds; GCompOpSet honors their alpha.
  graphics_context_set_compositing_mode(ctx, GCompOpSet);

  // Bottom-align every sprite on the ground line using its real size.
  GSize cat_size = cat ? gbitmap_get_bounds(cat).size : GSize(SPRITE_W, SPRITE_H);
  int cat_x = (SCREEN_W - cat_size.w) / 2;
  int cat_y = GROUND_OFFSET - cat_size.h + s_jump_offset;

  if (state->pet_state == PET_STATE_ENERGETIC && s_butterfly_bitmap) {
    GSize bf = gbitmap_get_bounds(s_butterfly_bitmap).size;
    int bx = cat_x + cat_size.w + BUTTERFLY_OFFSET_X + (s_anim_frame % 3) * 2;
    int by = cat_y - BUTTERFLY_OFFSET_Y + (s_anim_frame % 2) * 3;
    if (bx + bf.w > SCREEN_W) {
      bx = SCREEN_W - bf.w;
    }
    graphics_draw_bitmap_in_rect(ctx, s_butterfly_bitmap, GRect(bx, by, bf.w, bf.h));
  }

  if (cat) {
    graphics_draw_bitmap_in_rect(ctx, cat, GRect(cat_x, cat_y, cat_size.w, cat_size.h));
  }

  if (state->pet_state == PET_STATE_DEAD && s_ghost_bitmap) {
    GSize gs = gbitmap_get_bounds(s_ghost_bitmap).size;
    int ghost_y = cat_y - gs.h + GHOST_FLOAT_OFFSET + ((s_ghost_frame % 2) ? -GHOST_FLOAT_RANGE : GHOST_FLOAT_RANGE);
    if (ghost_y < 0) {
      ghost_y = 0;
    }
    graphics_draw_bitmap_in_rect(ctx, s_ghost_bitmap,
                                 GRect((SCREEN_W - gs.w) / 2, ghost_y, gs.w, gs.h));
  }

  if (s_flick_interrupt) {
    prv_draw_speech_bubble(ctx, cat_x, cat_y);
  }

  if (state->pet_state == PET_STATE_LOW_BATTERY) {
    prv_draw_battery_icon(ctx, cat_x + cat_size.w - BATTERY_OFFSET_X, cat_y - BATTERY_OFFSET_Y, s_battery_flash);
  }
}

static void prv_hud_update_proc(Layer *layer, GContext *ctx) {
  const GameState *state = game_get_state();
  GRect bounds = layer_get_bounds(layer);

  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_compositing_mode(ctx, GCompOpSet);

  GSize bar = s_fish_skeleton ? gbitmap_get_bounds(s_fish_skeleton).size : GSize(64, 16);
  int bar_x = bounds.size.w - bar.w - 6;
  int bar_y = (bounds.size.h - bar.h) / 2;
  int fill_w = (int)(bar.w * state->progress);
  if (fill_w < 0) {
    fill_w = 0;
  }
  if (fill_w > bar.w) {
    fill_w = bar.w;
  }

  if (s_fish_skeleton) {
    graphics_draw_bitmap_in_rect(ctx, s_fish_skeleton, GRect(bar_x, bar_y, bar.w, bar.h));
  }
  // Drawing the meat into a narrower rect clips it from the left,
  // revealing the fish progressively as steps accumulate.
  if (s_fish_meat && fill_w > 0) {
    graphics_draw_bitmap_in_rect(ctx, s_fish_meat, GRect(bar_x, bar_y, fill_w, bar.h));
  }
}

static void prv_update_clock(void) {
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);

  static char time_buffer[8];
  static char date_buffer[24];

  if (clock_is_24h_style()) {
    strftime(time_buffer, sizeof(time_buffer), "%H:%M", tm_info);
  } else {
    strftime(time_buffer, sizeof(time_buffer), "%I:%M", tm_info);
    if (time_buffer[0] == '0') {
      memmove(time_buffer, time_buffer + 1, strlen(time_buffer));
    }
  }
  text_layer_set_text(s_time_layer, time_buffer);

  strftime(date_buffer, sizeof(date_buffer), "%a %b %d", tm_info);
  text_layer_set_text(s_date_layer, date_buffer);
}

static void prv_update_steps_text(void) {
  const GameState *state = game_get_state();
  static char steps_buffer[16];
  if (state->is_dead) {
    snprintf(steps_buffer, sizeof(steps_buffer), "%ld/%d",
             (long)state->resurrection_progress, state->resurrection_goal);
  } else {
    snprintf(steps_buffer, sizeof(steps_buffer), "%ld", (long)state->steps);
  }
  text_layer_set_text(s_steps_layer, steps_buffer);
}

static void prv_refresh_ui(bool start_anim) {
  const GameState *state = game_get_state();
  const GameSettings *settings = game_get_settings();

  sprites_set_breed(settings->cat_breed);
  if (!s_anim_active) {
    sprites_load_frame(state->pet_state, 0);
  }

  prv_update_clock();
  prv_update_steps_text();
  s_battery_flash = !s_battery_flash;
  prv_request_redraw();

  if (start_anim) {
    prv_start_animation();
  }
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // game_update() detects day rollover itself (idempotent), so the
  // DAY_UNIT case needs no special handling here.
  game_update();
  prv_refresh_ui(units_changed & MINUTE_UNIT);
}

static void prv_health_handler(HealthEventType event, void *context) {
  if (event == HealthEventMovementUpdate) {
    game_update();
    prv_update_steps_text();
    prv_request_redraw();
  }
}

static void prv_battery_handler(BatteryChargeState battery) {
  game_update();
  prv_refresh_ui(false);
}

static void prv_accel_tap_handler(AccelAxisType axis, int32_t direction) {
  s_flick_interrupt = true;
  s_jump_offset = -FLICK_JUMP_HEIGHT;
  prv_start_animation();
  prv_request_redraw();

  app_timer_register(120, prv_flick_jump_cb, (void *)(intptr_t)0);
  app_timer_register(240, prv_flick_jump_cb, (void *)(intptr_t)1);
  app_timer_register(360, prv_flick_jump_cb, (void *)(intptr_t)2);
  app_timer_register(800, prv_flick_reset_cb, NULL);
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  game_on_settings_received(iter);
  const GameSettings *settings = game_get_settings();
  sprites_set_breed(settings->cat_breed);
  prv_refresh_ui(true);
}

static void prv_inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Inbox dropped: %d", reason);
}

static void prv_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);

  s_time_layer = text_layer_create(GRect(0, TIME_LAYER_Y, SCREEN_W, TIME_LAYER_H));
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_TIME));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  s_date_layer = text_layer_create(GRect(0, DATE_LAYER_Y, SCREEN_W, DATE_LAYER_H));
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_DATE));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer, GColorWhite);
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

  s_stage_layer = layer_create(GRect(0, HEADER_H, SCREEN_W, STAGE_H));
  layer_set_update_proc(s_stage_layer, prv_stage_update_proc);
  layer_add_child(window_layer, s_stage_layer);

  s_hud_layer = layer_create(GRect(0, HEADER_H + STAGE_H, SCREEN_W, HUD_H));
  layer_set_update_proc(s_hud_layer, prv_hud_update_proc);
  layer_add_child(window_layer, s_hud_layer);

  s_steps_layer = text_layer_create(GRect(STEPS_LAYER_X, HEADER_H + STAGE_H + STEPS_LAYER_Y_OFFSET,
                                         STEPS_LAYER_W, STEPS_LAYER_H));
  text_layer_set_font(s_steps_layer, fonts_get_system_font(FONT_STEPS));
  text_layer_set_background_color(s_steps_layer, GColorClear);
  text_layer_set_text_color(s_steps_layer, GColorWhite);
  layer_add_child(window_layer, text_layer_get_layer(s_steps_layer));

  s_fish_skeleton = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_FISH_SKELETON);
  s_fish_meat = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_FISH_MEAT);

  sprites_init();
  sprites_set_breed(game_get_settings()->cat_breed);
  game_update();
  prv_refresh_ui(true);
}

static void prv_window_unload(Window *window) {
  prv_stop_animation();
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_date_layer);
  text_layer_destroy(s_steps_layer);
  layer_destroy(s_stage_layer);
  layer_destroy(s_hud_layer);

  if (s_fish_skeleton) {
    gbitmap_destroy(s_fish_skeleton);
    s_fish_skeleton = NULL;
  }
  if (s_fish_meat) {
    gbitmap_destroy(s_fish_meat);
    s_fish_meat = NULL;
  }
  if (s_ghost_bitmap) {
    gbitmap_destroy(s_ghost_bitmap);
    s_ghost_bitmap = NULL;
  }
  if (s_butterfly_bitmap) {
    gbitmap_destroy(s_butterfly_bitmap);
    s_butterfly_bitmap = NULL;
  }
  sprites_deinit();
}

static void prv_init(void) {
  game_init();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT | DAY_UNIT, prv_tick_handler);
  health_service_events_subscribe(prv_health_handler, NULL);
  battery_state_service_subscribe(prv_battery_handler);
  accel_tap_service_subscribe(prv_accel_tap_handler);

  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static void prv_deinit(void) {
  accel_tap_service_unsubscribe();
  battery_state_service_unsubscribe();
  health_service_events_unsubscribe();
  tick_timer_service_unsubscribe();
  game_deinit();
  window_destroy(s_window);
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
