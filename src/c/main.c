// NineLives: a watchface whose cats live or die by your daily steps.
#include <pebble.h>
#include <stdlib.h>
#include "game.h"
#include "hud.h"
#include "layout.h"
#include "scene.h"
#include "sprites.h"
#ifdef NINELIVES_DEMO
#include "demo.h"
#endif

#define FRAME_MS 200
// Every minute the scene comes alive for a few seconds.
#define ANIM_FRAMES (4000 / FRAME_MS)
#define BUBBLE_FRAMES (2400 / FRAME_MS)

typedef struct {
  int16_t x;
  int16_t target_x;
  // The stretch of grass this cat wanders, so a group stays spread out.
  int16_t min_x;
  int16_t max_x;
  bool back;
  CatFacing facing;
  uint8_t frame_offset;
} Cat;

static Window *s_window;
static Layer *s_canvas;
static AppTimer *s_frame_timer;
static uint16_t s_frame;
static uint8_t s_anim_frames_left;
static uint8_t s_bubble_frames_left;
static int8_t s_hop_frame = -1;
static Cat s_cats[MAX_CATS];
static int s_laid_out_count = -1;

// A tap makes every cat hop.
static const int8_t HOP_OFFSETS[] = { -3 * PX, -5 * PX, -4 * PX, -2 * PX };

// --- Inputs (real, or canned scenes in demo builds) -------------------------

static const GameState *prv_state(void) {
#ifdef NINELIVES_DEMO
  return demo_state();
#else
  return game_get_state();
#endif
}

static const GameSettings *prv_settings(void) {
#ifdef NINELIVES_DEMO
  return demo_settings();
#else
  return game_get_settings();
#endif
}

static struct tm prv_now(void) {
#ifdef NINELIVES_DEMO
  return demo_now();
#else
  const time_t now = time(NULL);
  return *localtime(&now);
#endif
}

static BatteryChargeState prv_battery(void) {
#ifdef NINELIVES_DEMO
  return demo_battery();
#else
  return battery_state_service_peek();
#endif
}

static bool prv_bubble_visible(void) {
#ifdef NINELIVES_DEMO
  if (demo_bubble()) {
    return true;
  }
#endif
  return s_bubble_frames_left > 0;
}

// --- Cats -------------------------------------------------------------------

static bool prv_animating(void) {
  return s_anim_frames_left > 0;
}

static int prv_hop_offset(void) {
  return s_hop_frame >= 0 ? HOP_OFFSETS[s_hop_frame] : 0;
}

static int prv_min_x(void) {
  return CAT_MARGIN;
}

static int prv_max_x(void) {
  return SCREEN_W - CAT_W - CAT_MARGIN;
}

static void prv_pick_target(Cat *cat) {
  cat->target_x = (cat->min_x + rand() % (cat->max_x - cat->min_x + 1)) / PX * PX;
}

static void prv_layout_cats(int count) {
  // Each cat's stretch of the walkable width, in percent, and where it starts.
  // Extra cats stand behind the first, one to each side.
  typedef struct {
    uint8_t from;
    uint8_t to;
    uint8_t start;
  } Zone;
  static const Zone ZONES[MAX_CATS][MAX_CATS] = {
    { { 0, 100, 50 } },
    { { 0, 55, 26 }, { 45, 100, 78 } },
    { { 30, 70, 50 }, { 0, 22, 4 }, { 78, 100, 96 } },
  };
  if (count <= 0 || count > MAX_CATS) {
    return;
  }
  const int span = prv_max_x() - prv_min_x();
  for (int i = 0; i < count; i++) {
    Cat *cat = &s_cats[i];
    const Zone *zone = &ZONES[count - 1][i];
    cat->min_x = prv_min_x() + span * zone->from / 100;
    cat->max_x = prv_min_x() + span * zone->to / 100;
    cat->x = (prv_min_x() + span * zone->start / 100) / PX * PX;
    cat->back = i > 0;
    cat->facing = cat->x + CAT_W / 2 <= SCREEN_W / 2 ? CAT_FACING_RIGHT : CAT_FACING_LEFT;
    cat->frame_offset = i;
    prv_pick_target(cat);
  }
}

static void prv_sync_cats(void) {
  const int count = prv_state()->cat_count;
  if (count != s_laid_out_count) {
    prv_layout_cats(count);
    s_laid_out_count = count;
  }
}

static void prv_wander(Cat *cat) {
  if (cat->x == cat->target_x) {
    prv_pick_target(cat);
    return;
  }
  cat->facing = cat->x < cat->target_x ? CAT_FACING_RIGHT : CAT_FACING_LEFT;
  cat->x += cat->x < cat->target_x ? PX : -PX;
}

static CatPose prv_pose(PetState state) {
  switch (state) {
    case PET_STATE_ENERGETIC:
      return CAT_POSE_WALK;
    case PET_STATE_DISTRESSED:
      return CAT_POSE_DISTRESSED;
    case PET_STATE_SLEEPING:
      return CAT_POSE_SLEEP;
    case PET_STATE_CHARGING:
      return CAT_POSE_BOX;
    default:
      return CAT_POSE_SIT;
  }
}

static CatBreed prv_breed(const GameSettings *settings, int index) {
  return (CatBreed)((settings->cat_breed + index) % NUM_CAT_BREEDS);
}

static GRect prv_draw_cat(GContext *ctx, int index, const GameState *state,
                          const GameSettings *settings) {
  const Cat *cat = &s_cats[index];
  const CatPose pose = prv_pose(state->pet_state);
  const uint8_t frame = prv_animating()
      ? (s_frame + cat->frame_offset) % sprites_pose_frames(pose)
      : (pose == CAT_POSE_WALK ? 1 : 0);
  GBitmap *bitmap = sprites_cat(index, prv_breed(settings, index), pose, frame, cat->facing);
  if (!bitmap) {
    return GRectZero;
  }
  const GSize size = gbitmap_get_bounds(bitmap).size;
  const int lane_y = cat->back ? LANE_BACK_Y : LANE_FRONT_Y;
  const GRect frame_rect = GRect(cat->x, lane_y - size.h + prv_hop_offset(), size.w, size.h);
  graphics_draw_bitmap_in_rect(ctx, bitmap, frame_rect);
  return frame_rect;
}

static void prv_draw_butterfly(GContext *ctx, GRect cat) {
  GBitmap *bitmap = sprites_butterfly(s_frame);
  if (!bitmap) {
    return;
  }
  const GSize size = gbitmap_get_bounds(bitmap).size;
  const bool right = cat.origin.x + cat.size.w + size.w + 4 * PX < SCREEN_W;
  const int x = right ? cat.origin.x + cat.size.w - 2 * PX + (s_frame % 3) * PX
                      : cat.origin.x - size.w + 2 * PX - (s_frame % 3) * PX;
  const int y = cat.origin.y - 2 * PX - (s_frame % 2) * 2 * PX;
  graphics_draw_bitmap_in_rect(ctx, bitmap, GRect(x, y, size.w, size.h));
}

// Draws the cats, back row first; returns the main cat's frame.
static GRect prv_draw_cats(GContext *ctx, const GameState *state, const GameSettings *settings) {
  GRect main_cat = GRectZero;
  for (int pass = 0; pass < 2; pass++) {
    for (int i = 0; i < state->cat_count && i < MAX_CATS; i++) {
      if (s_cats[i].back == (pass == 0)) {
        const GRect rect = prv_draw_cat(ctx, i, state, settings);
        if (i == 0) {
          main_cat = rect;
        }
      }
    }
  }

  if (state->pet_state == PET_STATE_ENERGETIC && prv_animating()) {
    prv_draw_butterfly(ctx, main_cat);
  }
  if (state->pet_state == PET_STATE_LOW_BATTERY && (!prv_animating() || (s_frame / 2) % 2)) {
    const GSize size = hud_battery_size();
    hud_draw_battery(ctx, GPoint(main_cat.origin.x + main_cat.size.w / 2 - size.w / 2 + 2 * PX,
                                 main_cat.origin.y - size.h - PX),
                     prv_battery());
  }
  return main_cat;
}

// A tombstone with the cat's ghost bobbing above it; returns the ghost's frame.
static GRect prv_draw_grave(GContext *ctx) {
  GBitmap *stone = sprites_tombstone();
  if (!stone) {
    return GRectZero;
  }
  const GSize stone_size = gbitmap_get_bounds(stone).size;
  const int stone_x = (SCREEN_W - stone_size.w) / 2 / PX * PX;
  const int stone_y = LANE_FRONT_Y - stone_size.h;
  graphics_draw_bitmap_in_rect(ctx, stone, GRect(stone_x, stone_y, stone_size.w, stone_size.h));

  const uint8_t ghost_frame = prv_animating() ? s_frame / 2 : 0;
  GBitmap *ghost = sprites_ghost(ghost_frame);
  if (!ghost) {
    return GRect(stone_x, stone_y, stone_size.w, stone_size.h);
  }
  const GSize ghost_size = gbitmap_get_bounds(ghost).size;
  const GRect ghost_rect = GRect((SCREEN_W - ghost_size.w) / 2 / PX * PX,
                                 stone_y - ghost_size.h - (ghost_frame % 2 ? 2 : 1) * PX,
                                 ghost_size.w, ghost_size.h);
  graphics_draw_bitmap_in_rect(ctx, ghost, ghost_rect);
  return ghost_rect;
}

// --- Speech bubble ----------------------------------------------------------

static const char *prv_bubble_text(const GameState *state) {
  if (state->is_dead) {
    return "boo!";
  }
  switch (state->pet_state) {
    case PET_STATE_SLEEPING:
      return "zzz";
    case PET_STATE_CHARGING:
      return "cozy!";
    case PET_STATE_LOW_BATTERY:
      return "low!";
    case PET_STATE_DISTRESSED:
      return "walk!";
    case PET_STATE_LAZY:
      return "walk?";
    default:
      return state->steps >= state->step_goal ? "yum!" : "meow!";
  }
}

static int prv_clamp(int value, int low, int high) {
  return value < low ? low : (value > high ? high : value);
}

// Above the speaker when there's room under the clock, otherwise beside it.
static void prv_draw_bubble(GContext *ctx, GRect speaker, const char *text) {
  const GFont font = fonts_get_system_font(FONT_BUBBLE);
  const GSize text_size = graphics_text_layout_get_content_size(
      text, font, GRect(0, 0, SCREEN_W, 40), GTextOverflowModeFill, GTextAlignmentCenter);
  const int w = (text_size.w + 6 * PX) / PX * PX;
  const int h = (text_size.h + 2 * PX) / PX * PX;
  const int center_x = speaker.origin.x + speaker.size.w / 2;

  int x;
  int y = speaker.origin.y - h - PX;
  GRect tail;
  if (y >= CLOCK_BOTTOM) {
    x = prv_clamp(center_x - w / 2, 2 * PX, SCREEN_W - 2 * PX - w);
    tail = GRect(prv_clamp(center_x, x + 2 * PX, x + w - 3 * PX), y + h, PX, PX);
  } else {
    const bool right = speaker.origin.x + speaker.size.w + PX + w <= SCREEN_W - 2 * PX;
    x = prv_clamp(right ? speaker.origin.x + speaker.size.w + PX : speaker.origin.x - PX - w,
                  2 * PX, SCREEN_W - 2 * PX - w);
    y = speaker.origin.y < CLOCK_BOTTOM ? CLOCK_BOTTOM : speaker.origin.y;
    tail = GRect(right ? x - PX : x + w, y + h - 2 * PX, PX, PX);
  }

  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(x, y, w, h), PX * 2, GCornersAll);
  graphics_fill_rect(ctx, tail, 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, text, font, GRect(x, y - 1, w, h), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
}

// --- Drawing ----------------------------------------------------------------

static void prv_canvas_update(Layer *layer, GContext *ctx) {
  const GameState *state = prv_state();
  const struct tm now = prv_now();
  const ScenePalette *palette = scene_palette(scene_phase(&now));

  scene_draw(ctx, &now, prv_animating() && (s_frame / 2) % 2);
  hud_draw_status(ctx, &now, prv_battery(), palette);
  hud_draw_clock(ctx, &now, palette);

  graphics_context_set_compositing_mode(ctx, GCompOpSet);
  const GRect speaker = state->is_dead ? prv_draw_grave(ctx)
                                       : prv_draw_cats(ctx, state, prv_settings());
  graphics_context_set_compositing_mode(ctx, GCompOpAssign);

  hud_draw_panel(ctx, state);
  if (prv_bubble_visible()) {
    prv_draw_bubble(ctx, speaker, prv_bubble_text(state));
  }
}

// --- Animation --------------------------------------------------------------

static void prv_frame_tick(void *data);

static void prv_schedule_frame(void) {
  if (!s_frame_timer) {
    s_frame_timer = app_timer_register(FRAME_MS, prv_frame_tick, NULL);
  }
}

static void prv_animate(void) {
  s_anim_frames_left = ANIM_FRAMES;
  prv_schedule_frame();
  layer_mark_dirty(s_canvas);
}

static void prv_frame_tick(void *data) {
  s_frame_timer = NULL;
  s_frame++;
  if (s_hop_frame >= 0 && ++s_hop_frame >= (int)ARRAY_LENGTH(HOP_OFFSETS)) {
    s_hop_frame = -1;
  }
  if (s_bubble_frames_left > 0) {
    s_bubble_frames_left--;
  }
  if (s_anim_frames_left > 0) {
    s_anim_frames_left--;
    const GameState *state = prv_state();
    if (state->pet_state == PET_STATE_ENERGETIC) {
      for (int i = 0; i < state->cat_count && i < MAX_CATS; i++) {
        prv_wander(&s_cats[i]);
      }
    }
  }
  layer_mark_dirty(s_canvas);
  if (s_anim_frames_left > 0 || s_bubble_frames_left > 0 || s_hop_frame >= 0) {
    prv_schedule_frame();
  }
}

// --- Events -----------------------------------------------------------------

static void prv_refresh(void) {
  prv_sync_cats();
  layer_mark_dirty(s_canvas);
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  game_update();
  prv_refresh();
  // Let the battery rest while you sleep.
  if (prv_state()->pet_state != PET_STATE_SLEEPING) {
    prv_animate();
  }
}

static void prv_health_handler(HealthEventType event, void *context) {
  if (event == HealthEventMovementUpdate || event == HealthEventSignificantUpdate
      || event == HealthEventSleepUpdate) {
    game_update();
    prv_refresh();
  }
}

static void prv_battery_handler(BatteryChargeState charge) {
  game_update();
  prv_refresh();
}

static void prv_tap_handler(AccelAxisType axis, int32_t direction) {
#ifdef NINELIVES_DEMO
  demo_next();
  prv_refresh();
  return;
#endif
  s_hop_frame = 0;
  s_bubble_frames_left = BUBBLE_FRAMES;
  prv_animate();
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  game_on_settings_received(iter);
  prv_refresh();
  prv_animate();
}

static void prv_inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Inbox dropped: %d", reason);
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, prv_canvas_update);
  layer_add_child(root, s_canvas);

  srand((unsigned int)time(NULL));
  scene_init();
  prv_refresh();
  prv_animate();
}

static void prv_window_unload(Window *window) {
  if (s_frame_timer) {
    app_timer_cancel(s_frame_timer);
    s_frame_timer = NULL;
  }
  layer_destroy(s_canvas);
  sprites_deinit();
}

static void prv_init(void) {
  game_init();
#ifdef NINELIVES_DEMO
  demo_init();
#endif

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_handler);
  health_service_events_subscribe(prv_health_handler, NULL);
  battery_state_service_subscribe(prv_battery_handler);
  accel_tap_service_subscribe(prv_tap_handler);

  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_open(256, 64);
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
