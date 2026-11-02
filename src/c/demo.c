#include "demo.h"

#ifdef NINELIVES_DEMO

typedef struct {
  uint8_t hour;
  uint8_t minute;
  int32_t steps;
  int32_t goal;
  uint8_t cats;
  PetState pet;
  PetState pace;
  uint8_t battery;
  bool charging;
  CatBreed breed;
  bool forgiving;
  bool bubble;
  int8_t trend;
} DemoScene;

static const DemoScene SCENES[] = {
  { 10, 24, 4210, 8000, 1, PET_STATE_ENERGETIC, PET_STATE_ENERGETIC, 80, false,
    CAT_BREED_ORANGE, false, false, 1 },
  { 6, 35, 640, 7600, 2, PET_STATE_LAZY, PET_STATE_LAZY, 64, false,
    CAT_BREED_TUXEDO, false, false, -1 },
  { 18, 5, 2100, 10000, 3, PET_STATE_DISTRESSED, PET_STATE_DISTRESSED, 45, false,
    CAT_BREED_SIAMESE, true, true, 0 },
  { 23, 40, 9120, 8000, 3, PET_STATE_SLEEPING, PET_STATE_ENERGETIC, 30, false,
    CAT_BREED_ORANGE, false, false, 1 },
  { 13, 10, 5230, 8000, 2, PET_STATE_CHARGING, PET_STATE_ENERGETIC, 55, true,
    CAT_BREED_VOID, false, false, 0 },
  { 16, 45, 6020, 8000, 1, PET_STATE_LOW_BATTERY, PET_STATE_LAZY, 8, false,
    CAT_BREED_ORANGE, false, false, 0 },
  { 21, 15, 230, 8000, 0, PET_STATE_DEAD, PET_STATE_DISTRESSED, 70, false,
    CAT_BREED_ORANGE, false, true, 0 },
  { 12, 0, 10250, 8400, 3, PET_STATE_ENERGETIC, PET_STATE_ENERGETIC, 90, false,
    CAT_BREED_ORANGE, false, true, 1 },
};

static unsigned s_index;
static GameState s_state;
static GameSettings s_settings;

static void prv_apply(void) {
  const DemoScene *scene = &SCENES[s_index];
  const bool dead = scene->cats == 0;
  s_settings = (GameSettings) {
    .step_goal = scene->goal,
    .cat_breed = scene->breed,
    .difficulty = scene->forgiving ? DIFFICULTY_FORGIVING : DIFFICULTY_STRICT,
    .active_start = DEFAULT_ACTIVE_START,
    .active_end = DEFAULT_ACTIVE_END,
  };
  s_state = (GameState) {
    .steps = scene->steps,
    .step_goal = scene->goal,
    .survival_steps = scene->forgiving ? scene->goal / 2 : scene->goal,
    .progress = (float)scene->steps / (float)scene->goal,
    .pet_state = scene->pet,
    .pace_state = scene->pace,
    .cat_count = scene->cats,
    .is_dead = dead,
    .is_charging = scene->charging,
    .is_low_battery = scene->battery < LOW_BATTERY_THRESHOLD,
    .resurrection_progress = dead ? scene->steps : 0,
    .resurrection_goal = RESURRECTION_STEPS,
    .goal_trend = scene->trend,
  };
}

void demo_init(void) {
  s_index = (unsigned)(NINELIVES_DEMO) % ARRAY_LENGTH(SCENES);
  prv_apply();
}

void demo_next(void) {
  s_index = (s_index + 1) % ARRAY_LENGTH(SCENES);
  prv_apply();
}

const GameState *demo_state(void) {
  return &s_state;
}

const GameSettings *demo_settings(void) {
  return &s_settings;
}

struct tm demo_now(void) {
  time_t now = time(NULL);
  struct tm result = *localtime(&now);
  result.tm_hour = SCENES[s_index].hour;
  result.tm_min = SCENES[s_index].minute;
  return result;
}

BatteryChargeState demo_battery(void) {
  return (BatteryChargeState) {
    .charge_percent = SCENES[s_index].battery,
    .is_charging = SCENES[s_index].charging,
    .is_plugged = SCENES[s_index].charging,
  };
}

bool demo_bubble(void) {
  return SCENES[s_index].bubble;
}

#endif
