#pragma once

#include <pebble.h>

#define DEFAULT_STEP_GOAL 10000
#define MIN_AUTO_GOAL 1000
#define MAX_STEP_GOAL 100000
#define DEFAULT_ACTIVE_START 8
#define DEFAULT_ACTIVE_END 22
#define RESURRECTION_STEPS 1000
#define LOW_BATTERY_THRESHOLD 10
#define MAX_CATS 3

#define PERSIST_KEY_STEP_GOAL 1
#define PERSIST_KEY_CAT_BREED 2
#define PERSIST_KEY_DIFFICULTY 3
#define PERSIST_KEY_ACTIVE_START 4
#define PERSIST_KEY_ACTIVE_END 5
#define PERSIST_KEY_IS_DEAD 6
#define PERSIST_KEY_LAST_EVAL_DAY 7
#define PERSIST_KEY_LAST_DAY_STEPS 8
#define PERSIST_KEY_CAT_COUNT 9
#define PERSIST_KEY_LAST_REWARD_DAY 10
#define PERSIST_KEY_CACHED_GOAL 11
#define PERSIST_KEY_CACHED_GOAL_DAY 12
#define PERSIST_KEY_REVIVED_DAY 13
#define PERSIST_KEY_SCHEMA_VERSION 14
#define PERSIST_KEY_SMART_LEVEL 15
#define PERSIST_KEY_PREVIOUS_GOAL 16

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

typedef struct {
  // 0 means the smart goal, which adapts to recent days.
  int32_t step_goal;
  CatBreed cat_breed;
  Difficulty difficulty;
  int active_start;
  int active_end;
} GameSettings;

typedef struct {
  int32_t steps;
  int32_t step_goal;
  // Steps needed today to keep from losing a cat (half the goal on Forgiving).
  int32_t survival_steps;
  // Where the pace line says the step count should be by now.
  int32_t expected_steps;
  float progress;
  PetState pet_state;
  // Energetic, lazy or distressed: how the day is going, even while the cat
  // is shown sleeping or charging instead.
  PetState pace_state;
  int cat_count;
  bool is_dead;
  bool is_sleeping;
  bool is_charging;
  bool is_low_battery;
  int32_t resurrection_progress;
  int resurrection_goal;
  // +1 or -1 when the smart goal moved since yesterday, 0 otherwise.
  int8_t goal_trend;
} GameState;

void game_init(void);
void game_deinit(void);
// Re-reads steps, battery and sleep, and settles any finished days.
void game_update(void);
void game_on_settings_received(DictionaryIterator *iter);

const GameSettings *game_get_settings(void);
const GameState *game_get_state(void);

int game_get_day_number(void);
