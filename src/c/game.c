#include "game.h"

static GameSettings s_settings;
static GameState s_state;
static int s_current_day = -1;

#ifdef STEP_OR_DED_DEBUG
static bool s_debug_force_state = false;
static PetState s_debug_state = PET_STATE_ENERGETIC;
static bool s_debug_force_steps = false;
static int32_t s_debug_steps = 0;
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

int game_get_day_number(void) {
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  return tm_info->tm_year * 400 + tm_info->tm_yday;
}

int32_t game_get_step_goal(void) {
  if (s_settings.step_goal > 0) {
    return s_settings.step_goal;
  }

  time_t now = time(NULL);
  time_t start = now - (30 * SECONDS_PER_DAY);
  HealthValue avg = health_service_sum_averaged(
      HealthMetricStepCount, start, now, HealthServiceTimeScopeDaily);
  if (avg > 0) {
    return (int32_t)avg;
  }
  return DEFAULT_STEP_GOAL;
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

SkyPhase game_get_sky_phase(int hour) {
  if (hour >= 6 && hour < 17) {
    return SKY_DAY;
  }
  if (hour >= 17 && hour < 20) {
    return SKY_SUNSET;
  }
  return SKY_NIGHT;
}

void game_on_day_change(void) {
  int32_t goal = game_get_step_goal();
  int last_steps = persist_exists(PERSIST_KEY_LAST_DAY_STEPS)
      ? persist_read_int(PERSIST_KEY_LAST_DAY_STEPS) : 0;

  if (prv_goal_failed(last_steps, goal)) {
    s_state.is_dead = true;
    persist_write_bool(PERSIST_KEY_IS_DEAD, true);
  } else if (!s_state.is_dead) {
    persist_write_bool(PERSIST_KEY_IS_DEAD, false);
  }

  persist_write_int(PERSIST_KEY_LAST_EVAL_DAY, s_current_day);
  persist_write_int(PERSIST_KEY_LAST_DAY_STEPS, 0);
  s_state.resurrection_progress = 0;
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

void game_update(void) {
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  int day = game_get_day_number();

  if (s_current_day < 0) {
    s_current_day = day;
    if (persist_exists(PERSIST_KEY_LAST_EVAL_DAY)) {
      int last_eval = persist_read_int(PERSIST_KEY_LAST_EVAL_DAY);
      if (last_eval != day) {
        game_on_day_change();
      }
    }
  } else if (day != s_current_day) {
    game_on_day_change();
    s_current_day = day;
  }

#ifdef STEP_OR_DED_DEBUG
  if (s_debug_force_steps) {
    s_state.steps = s_debug_steps;
  } else {
    s_state.steps = (int32_t)health_service_sum_today(HealthMetricStepCount);
  }
#else
  s_state.steps = (int32_t)health_service_sum_today(HealthMetricStepCount);
#endif

  persist_write_int(PERSIST_KEY_LAST_DAY_STEPS, s_state.steps);

  s_state.step_goal = game_get_step_goal();
  s_state.sky_phase = game_get_sky_phase(tm_info->tm_hour);

  prv_update_sleep_state();
  prv_update_battery_state();

  if (s_state.is_dead) {
    s_state.resurrection_progress = s_state.steps;
    if (s_state.resurrection_progress >= RESURRECTION_STEPS) {
      s_state.is_dead = false;
      persist_write_bool(PERSIST_KEY_IS_DEAD, false);
      s_state.resurrection_progress = 0;
    }
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
  s_state.is_dead = persist_exists(PERSIST_KEY_IS_DEAD) && persist_read_bool(PERSIST_KEY_IS_DEAD);
  s_state.resurrection_goal = RESURRECTION_STEPS;
  s_current_day = game_get_day_number();
  game_update();
}

void game_deinit(void) {
}

void game_reload_settings(void) {
  prv_load_settings();
  game_update();
}

void game_on_settings_received(DictionaryIterator *iter) {
  Tuple *tuple = NULL;

  tuple = dict_find(iter, MESSAGE_KEY_StepGoal);
  if (tuple) {
    s_settings.step_goal = tuple->value->int32;
    persist_write_int(PERSIST_KEY_STEP_GOAL, s_settings.step_goal);
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
#endif
