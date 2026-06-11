#pragma once

#include <pebble.h>

#define SCREEN_W 144
#define SCREEN_H 168
#define HEADER_H 52
#define STAGE_H 76
#define HUD_H 40
// Ground line, relative to the stage layer's coordinate space.
#define GROUND_OFFSET (STAGE_H - 10)

#define DEFAULT_STEP_GOAL 10000
#define DEFAULT_ACTIVE_START 8
#define DEFAULT_ACTIVE_END 22
#define RESURRECTION_STEPS 1000
#define LOW_BATTERY_THRESHOLD 10

#define PERSIST_KEY_STEP_GOAL 1
#define PERSIST_KEY_CAT_BREED 2
#define PERSIST_KEY_DIFFICULTY 3
#define PERSIST_KEY_ACTIVE_START 4
#define PERSIST_KEY_ACTIVE_END 5
#define PERSIST_KEY_IS_DEAD 6
#define PERSIST_KEY_LAST_EVAL_DAY 7
#define PERSIST_KEY_LAST_DAY_STEPS 8

typedef enum {
  CAT_BREED_ORANGE = 0,
  CAT_BREED_TUXEDO = 1,
  CAT_BREED_SIAMESE = 2,
  CAT_BREED_VOID = 3,
  NUM_CAT_BREEDS = 4
} CatBreed;

typedef enum {
  DIFFICULTY_FORGIVING = 0,
  DIFFICULTY_STRICT = 1
} Difficulty;

typedef enum {
  PET_STATE_ENERGETIC = 0,
  PET_STATE_LAZY,
  PET_STATE_DISTRESSED,
  PET_STATE_SLEEPING,
  PET_STATE_DEAD,
  PET_STATE_CHARGING,
  PET_STATE_LOW_BATTERY,
  NUM_PET_STATES
} PetState;

typedef enum {
  SKY_DAY = 0,
  SKY_SUNSET,
  SKY_NIGHT
} SkyPhase;

typedef struct {
  int32_t step_goal;
  CatBreed cat_breed;
  Difficulty difficulty;
  int active_start;
  int active_end;
} GameSettings;

typedef struct {
  int32_t steps;
  int32_t step_goal;
  int32_t expected_steps;
  float progress;
  float pace_ratio;
  PetState pet_state;
  SkyPhase sky_phase;
  bool is_dead;
  bool is_sleeping;
  bool is_charging;
  bool is_low_battery;
  bool pace_paused;
  int32_t resurrection_progress;
  int resurrection_goal;
} GameState;

void game_init(void);
void game_deinit(void);
void game_reload_settings(void);
void game_update(void);
void game_on_day_change(void);
void game_on_settings_received(DictionaryIterator *iter);

const GameSettings *game_get_settings(void);
const GameState *game_get_state(void);

int32_t game_get_step_goal(void);
PetState game_compute_pace_state(int32_t steps, int32_t goal);
SkyPhase game_get_sky_phase(int hour);
int game_get_day_number(void);

#ifdef STEP_OR_DED_DEBUG
void game_debug_force_state(PetState state);
void game_debug_force_steps(int32_t steps);
#endif
