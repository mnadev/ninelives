#include "game.h"

static GameSettings s_settings;
static GameState s_state;
static int s_current_day = -1;
static int32_t s_last_persisted_steps = -1;

#ifdef STEP_OR_DED_DEBUG
static bool s_debug_force_state = false;
static PetState s_debug_state = PET_STATE_ENERGETIC;
static bool s_debug_force_steps = false;
static int32_t s_debug_steps = 0;
static bool s_debug_force_cat_count = false;
static int s_debug_cat_count = 1;
#endif

static void prv_load_settings(void) {
  s_settings.step_goal = persist_exists(PERSIST_KEY_STEP_GOAL)
      ? persist_read_int(PERSIST_KEY_STEP_GOAL) : 0;
  s_settings.cat_breed = persist_exists(PERSIST_KEY_CAT_BREED)
      ? (CatBreed)persist_read_int(PERSIST_KEY_CAT_BREED) : CAT_BREED_ORANGE;
  s_settings.difficulty = persist_exists(PERSIST_KEY_DIFFICULTY)
      ? (Difficulty)persist_read_int(PERSIST_KEY_DIFFICULTY) : DIFFICULTY_STRICT;
  s_settings.active_start = persist_exists(PERSIST_KEY_ACTIVE_START)
      ? persist_read_int(PERSIST_KEY_ACTIVE_START) : DEFAULT_ACTIVE_START;
  s_settings.active_end = persist_exists(PERSIST_KEY_ACTIVE_END)
      ? persist_read_int(PERSIST_KEY_ACTIVE_END) : DEFAULT_ACTIVE_END;

  if (s_settings.cat_breed >= NUM_CAT_BREEDS) {
    s_settings.cat_breed = CAT_BREED_ORANGE;
  }
  if (s_settings.active_start < 0 || s_settings.active_start > 23) {
    s_settings.active_start = DEFAULT_ACTIVE_START;
  }
  if (s_settings.active_end < 1 || s_settings.active_end > 24) {
    s_settings.active_end = DEFAULT_ACTIVE_END;
  }
  if (s_settings.active_end <= s_settings.active_start) {
    s_settings.active_start = DEFAULT_ACTIVE_START;
    s_settings.active_end = DEFAULT_ACTIVE_END;
  }
}

static bool prv_goal_failed(int32_t steps, int32_t goal) {
  if (goal <= 0) {
    return false;
  }
  if (s_settings.difficulty == DIFFICULTY_FORGIVING) {
    return steps < (goal / 2);
  }
  return steps < goal;
}

static int32_t prv_resolve_auto_goal(void) {
  time_t now = time(NULL);
  time_t start = now - (30 * SECONDS_PER_DAY);
  HealthValue avg = health_service_sum_averaged(
      HealthMetricStepCount, start, now, HealthServiceTimeScopeDaily);
  if (avg > 0) {
    return (int32_t)avg;
  }
  if (persist_exists(PERSIST_KEY_CACHED_GOAL)) {
    return persist_read_int(PERSIST_KEY_CACHED_GOAL);
  }
  return DEFAULT_STEP_GOAL;
}

static int32_t prv_refresh_cached_goal(void) {
  int today = game_get_day_number();
  if (persist_exists(PERSIST_KEY_CACHED_GOAL_DAY)
      && persist_read_int(PERSIST_KEY_CACHED_GOAL_DAY) == today) {
    return persist_read_int(PERSIST_KEY_CACHED_GOAL);
  }

  int32_t goal;
  if (s_settings.step_goal > 0) {
    goal = s_settings.step_goal;
  } else {
    goal = prv_resolve_auto_goal();
  }

  persist_write_int(PERSIST_KEY_CACHED_GOAL, goal);
  persist_write_int(PERSIST_KEY_CACHED_GOAL_DAY, today);
  return goal;
}

static int32_t prv_goal_for_day(int day) {
  if (persist_exists(PERSIST_KEY_CACHED_GOAL_DAY)
      && persist_read_int(PERSIST_KEY_CACHED_GOAL_DAY) == day
      && persist_exists(PERSIST_KEY_CACHED_GOAL)) {
    return persist_read_int(PERSIST_KEY_CACHED_GOAL);
  }
  if (s_settings.step_goal > 0) {
    return s_settings.step_goal;
  }
  if (persist_exists(PERSIST_KEY_CACHED_GOAL)) {
    return persist_read_int(PERSIST_KEY_CACHED_GOAL);
  }
  return DEFAULT_STEP_GOAL;
}

static void prv_sync_dead_state(void) {
  s_state.is_dead = s_state.cat_count <= 0;
}

static void prv_persist_cat_count(void) {
  persist_write_int(PERSIST_KEY_CAT_COUNT, s_state.cat_count);
  persist_write_bool(PERSIST_KEY_IS_DEAD, s_state.cat_count <= 0);
}

int game_get_day_number(void) {
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  return tm_info->tm_year * 400 + tm_info->tm_yday;
}

int32_t game_get_step_goal(void) {
  return prv_refresh_cached_goal();
}

CatBreed game_get_cat_breed_for_slot(int slot) {
  if (slot < 0) {
    slot = 0;
  }
  return (CatBreed)((s_settings.cat_breed + slot) % NUM_CAT_BREEDS);
}

void game_invalidate_goal_cache(void) {
  if (persist_exists(PERSIST_KEY_CACHED_GOAL_DAY)) {
    persist_delete(PERSIST_KEY_CACHED_GOAL_DAY);
  }
}

static float prv_get_pace_fraction(void) {
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  int current_minutes = tm_info->tm_hour * 60 + tm_info->tm_min;
  int start_minutes = s_settings.active_start * 60;
  int end_minutes = s_settings.active_end * 60;
  int active_minutes = end_minutes - start_minutes;

  if (active_minutes <= 0) {
    return 1.0f;
  }
  if (current_minutes <= start_minutes) {
    return 0.0f;
  }
  if (current_minutes >= end_minutes) {
    return 1.0f;
  }
  return (float)(current_minutes - start_minutes) / (float)active_minutes;
}

PetState game_compute_pace_state(int32_t steps, int32_t goal) {
  int32_t expected = (int32_t)((float)goal * prv_get_pace_fraction());
  if (expected <= 0) {
    return PET_STATE_ENERGETIC;
  }

  float ratio = (float)steps / (float)expected;
  s_state.expected_steps = expected;
  s_state.pace_ratio = ratio;

  if (ratio >= 1.0f) {
    return PET_STATE_ENERGETIC;
  }
  if (ratio >= 0.8f) {
    return PET_STATE_LAZY;
  }
  return PET_STATE_DISTRESSED;
}

// Evaluates the previous day's goal exactly once per day boundary.
// Safe to call repeatedly; guarded by the persisted last-evaluated day.
void game_on_day_change(void) {
  int today = game_get_day_number();
  int last_eval = persist_exists(PERSIST_KEY_LAST_EVAL_DAY)
      ? persist_read_int(PERSIST_KEY_LAST_EVAL_DAY) : -1;

  s_current_day = today;
  if (last_eval == today) {
    return;
  }

  // Only judge the cat if there was a tracked previous day (not first run).
  if (last_eval >= 0 && s_state.cat_count > 0) {
    int32_t goal = prv_goal_for_day(last_eval);
    int32_t last_steps = persist_exists(PERSIST_KEY_LAST_DAY_STEPS)
        ? persist_read_int(PERSIST_KEY_LAST_DAY_STEPS) : 0;
    if (prv_goal_failed(last_steps, goal)) {
      s_state.cat_count--;
      prv_persist_cat_count();
    }
  }

  persist_write_int(PERSIST_KEY_LAST_EVAL_DAY, today);
  persist_write_int(PERSIST_KEY_LAST_DAY_STEPS, 0);
  s_last_persisted_steps = 0;
  s_state.resurrection_progress = 0;
  prv_sync_dead_state();
  prv_refresh_cached_goal();
}

static void prv_update_sleep_state(void) {
  HealthActivityMask activities = health_service_peek_current_activities();
  s_state.is_sleeping = (activities & HealthActivitySleep) != 0;
}

static void prv_update_battery_state(void) {
  BatteryChargeState battery = battery_state_service_peek();
  s_state.is_charging = battery.is_plugged || battery.is_charging;
  s_state.is_low_battery = battery.charge_percent < LOW_BATTERY_THRESHOLD;
  s_state.pace_paused = s_state.is_low_battery;
}

static PetState prv_resolve_pet_state(void) {
#ifdef STEP_OR_DED_DEBUG
  if (s_debug_force_state) {
    return s_debug_state;
  }
#endif

  if (s_state.is_charging) {
    return PET_STATE_CHARGING;
  }
  if (s_state.is_low_battery) {
    return PET_STATE_LOW_BATTERY;
  }
  if (s_state.is_dead) {
    return PET_STATE_DEAD;
  }
  if (s_state.is_sleeping) {
    return PET_STATE_SLEEPING;
  }
  if (s_state.pace_paused) {
    return PET_STATE_LAZY;
  }
  return game_compute_pace_state(s_state.steps, s_state.step_goal);
}

static void prv_try_award_cat(void) {
  int today = game_get_day_number();
  int last_reward = persist_exists(PERSIST_KEY_LAST_REWARD_DAY)
      ? persist_read_int(PERSIST_KEY_LAST_REWARD_DAY) : -1;

  if (last_reward == today) {
    return;
  }
  if (s_state.step_goal <= 0 || s_state.steps < s_state.step_goal) {
    return;
  }
  if (s_state.cat_count >= MAX_CATS) {
    return;
  }

  s_state.cat_count++;
  prv_persist_cat_count();
  persist_write_int(PERSIST_KEY_LAST_REWARD_DAY, today);
  prv_sync_dead_state();
}

void game_update(void) {
  int day = game_get_day_number();

  if (day != s_current_day) {
    game_on_day_change();
  }

#ifdef STEP_OR_DED_DEBUG
  if (s_debug_force_steps) {
    s_state.steps = s_debug_steps;
  } else {
    s_state.steps = (int32_t)health_service_sum_today(HealthMetricStepCount);
  }
  if (s_debug_force_cat_count) {
    s_state.cat_count = s_debug_cat_count;
    prv_persist_cat_count();
    prv_sync_dead_state();
  }
#else
  s_state.steps = (int32_t)health_service_sum_today(HealthMetricStepCount);
#endif

  // Monotonic within a day: avoid clobbering the snapshot with a transient 0
  // while the health service is still loading after reboot.
  if (s_state.steps > s_last_persisted_steps) {
    persist_write_int(PERSIST_KEY_LAST_DAY_STEPS, s_state.steps);
    s_last_persisted_steps = s_state.steps;
  }

  s_state.step_goal = game_get_step_goal();

  prv_update_sleep_state();
  prv_update_battery_state();

  if (s_state.is_dead) {
    s_state.resurrection_progress = s_state.steps;
    if (s_state.resurrection_progress >= RESURRECTION_STEPS) {
      s_state.cat_count = 1;
      prv_persist_cat_count();
      s_state.resurrection_progress = 0;
      prv_sync_dead_state();
    }
  } else {
    prv_try_award_cat();
  }

  if (s_state.is_dead) {
    s_state.progress = (float)s_state.resurrection_progress / (float)RESURRECTION_STEPS;
    if (s_state.progress > 1.0f) {
      s_state.progress = 1.0f;
    }
  } else if (s_state.step_goal > 0) {
    s_state.progress = (float)s_state.steps / (float)s_state.step_goal;
    if (s_state.progress > 1.0f) {
      s_state.progress = 1.0f;
    }
  } else {
    s_state.progress = 0.0f;
  }

  s_state.pet_state = prv_resolve_pet_state();
}

void game_init(void) {
  prv_load_settings();
  memset(&s_state, 0, sizeof(s_state));
  s_state.resurrection_goal = RESURRECTION_STEPS;

  if (persist_exists(PERSIST_KEY_CAT_COUNT)) {
    s_state.cat_count = persist_read_int(PERSIST_KEY_CAT_COUNT);
  } else if (persist_exists(PERSIST_KEY_IS_DEAD) && persist_read_bool(PERSIST_KEY_IS_DEAD)) {
    s_state.cat_count = 0;
  } else {
    s_state.cat_count = 1;
  }
  if (s_state.cat_count < 0) {
    s_state.cat_count = 0;
  }
  if (s_state.cat_count > MAX_CATS) {
    s_state.cat_count = MAX_CATS;
  }
  prv_persist_cat_count();
  prv_sync_dead_state();

  if (persist_exists(PERSIST_KEY_LAST_DAY_STEPS)) {
    s_last_persisted_steps = persist_read_int(PERSIST_KEY_LAST_DAY_STEPS);
  } else {
    s_last_persisted_steps = 0;
  }

  // Leave s_current_day at -1 so the first game_update() evaluates any
  // missed midnight rollover (e.g. watchface was not running at midnight).
  s_current_day = -1;
  game_update();
}

void game_deinit(void) {
}

void game_reload_settings(void) {
  prv_load_settings();
  game_invalidate_goal_cache();
  game_update();
}

void game_on_settings_received(DictionaryIterator *iter) {
  Tuple *tuple = NULL;

  tuple = dict_find(iter, MESSAGE_KEY_StepGoal);
  if (tuple) {
    s_settings.step_goal = tuple->value->int32;
    persist_write_int(PERSIST_KEY_STEP_GOAL, s_settings.step_goal);
    game_invalidate_goal_cache();
  }

  tuple = dict_find(iter, MESSAGE_KEY_CatBreed);
  if (tuple) {
    s_settings.cat_breed = (CatBreed)tuple->value->int32;
    if (s_settings.cat_breed >= NUM_CAT_BREEDS) {
      s_settings.cat_breed = CAT_BREED_ORANGE;
    }
    persist_write_int(PERSIST_KEY_CAT_BREED, s_settings.cat_breed);
  }

  tuple = dict_find(iter, MESSAGE_KEY_Difficulty);
  if (tuple) {
    s_settings.difficulty = (Difficulty)tuple->value->int32;
    persist_write_int(PERSIST_KEY_DIFFICULTY, s_settings.difficulty);
  }

  tuple = dict_find(iter, MESSAGE_KEY_ActiveStart);
  if (tuple) {
    s_settings.active_start = tuple->value->int32;
    persist_write_int(PERSIST_KEY_ACTIVE_START, s_settings.active_start);
  }

  tuple = dict_find(iter, MESSAGE_KEY_ActiveEnd);
  if (tuple) {
    s_settings.active_end = tuple->value->int32;
    persist_write_int(PERSIST_KEY_ACTIVE_END, s_settings.active_end);
  }

  game_reload_settings();
}

const GameSettings *game_get_settings(void) {
  return &s_settings;
}

const GameState *game_get_state(void) {
  return &s_state;
}

#ifdef STEP_OR_DED_DEBUG
void game_debug_force_state(PetState state) {
  s_debug_force_state = true;
  s_debug_state = state;
  game_update();
}

void game_debug_force_steps(int32_t steps) {
  s_debug_force_steps = true;
  s_debug_steps = steps;
  game_update();
}

void game_debug_force_cat_count(int count) {
  s_debug_force_cat_count = true;
  if (count < 0) {
    count = 0;
  }
  if (count > MAX_CATS) {
    count = MAX_CATS;
  }
  s_debug_cat_count = count;
  game_update();
}
#endif
