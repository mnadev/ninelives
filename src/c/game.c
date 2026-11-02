#include "game.h"

// Bumped when persisted data from older versions needs cleaning up.
#define SCHEMA_VERSION 2
// For a moment after midnight Health can still report yesterday's count as
// today's, so the rollover waits this long before judging or rewarding.
#define ROLLOVER_GRACE_S (2 * SECONDS_PER_MINUTE)
// Finished days to catch up on when the watchface was not running.
#define MAX_CATCHUP_DAYS 7
// The fallback step snapshot is rewritten after this many new steps.
#define SNAPSHOT_STEP_DELTA 100

// Smart goal: how far back to look for a typical day.
#define SMART_HISTORY_DAYS 28
// Days under this were probably spent without the watch on.
#define SMART_WORN_STEPS 1000
// Weekdays and weekends get their own typical day once there are this many.
#define SMART_SAME_KIND_DAYS 4
// Fewer usable days than this falls back to the Health app's average.
#define SMART_MIN_DAYS 3
// Each step of the level moves the goal 5% of a typical day, from 80% to 110%.
#define SMART_LEVEL_MIN -2
#define SMART_LEVEL_MAX 4

static GameSettings s_settings;
static GameState s_state;
static int s_current_day = -1;
static int32_t s_last_persisted_steps = -1;

// --- Settings ---------------------------------------------------------------

static int32_t prv_read_int_or(uint32_t key, int32_t fallback) {
  return persist_exists(key) ? persist_read_int(key) : fallback;
}

static void prv_sanitize_settings(GameSettings *settings) {
  if (settings->step_goal < 0 || settings->step_goal > MAX_STEP_GOAL) {
    settings->step_goal = 0;
  }
  // The enum may be unsigned on the watch, so negative values wrap high.
  if ((unsigned)settings->cat_breed >= NUM_CAT_BREEDS) {
    settings->cat_breed = CAT_BREED_ORANGE;
  }
  if (settings->difficulty != DIFFICULTY_FORGIVING) {
    settings->difficulty = DIFFICULTY_STRICT;
  }
  if (settings->active_start < 0 || settings->active_end > 24
      || settings->active_end <= settings->active_start) {
    settings->active_start = DEFAULT_ACTIVE_START;
    settings->active_end = DEFAULT_ACTIVE_END;
  }
}

static void prv_load_settings(void) {
  s_settings = (GameSettings) {
    .step_goal = prv_read_int_or(PERSIST_KEY_STEP_GOAL, 0),
    .cat_breed = (CatBreed)prv_read_int_or(PERSIST_KEY_CAT_BREED, CAT_BREED_ORANGE),
    .difficulty = (Difficulty)prv_read_int_or(PERSIST_KEY_DIFFICULTY, DIFFICULTY_STRICT),
    .active_start = prv_read_int_or(PERSIST_KEY_ACTIVE_START, DEFAULT_ACTIVE_START),
    .active_end = prv_read_int_or(PERSIST_KEY_ACTIVE_END, DEFAULT_ACTIVE_END),
  };
  prv_sanitize_settings(&s_settings);
}

static void prv_save_settings(void) {
  persist_write_int(PERSIST_KEY_STEP_GOAL, s_settings.step_goal);
  persist_write_int(PERSIST_KEY_CAT_BREED, s_settings.cat_breed);
  persist_write_int(PERSIST_KEY_DIFFICULTY, s_settings.difficulty);
  persist_write_int(PERSIST_KEY_ACTIVE_START, s_settings.active_start);
  persist_write_int(PERSIST_KEY_ACTIVE_END, s_settings.active_end);
}

// Clay sends selects and text inputs as strings, sliders as integers.
static bool prv_tuple_int(const Tuple *tuple, int32_t *out) {
  if (!tuple) {
    return false;
  }
  switch (tuple->type) {
    case TUPLE_CSTRING:
      *out = atoi(tuple->value->cstring);
      return true;
    case TUPLE_INT:
      *out = tuple->length == 1 ? tuple->value->int8
           : tuple->length == 2 ? tuple->value->int16
           : tuple->value->int32;
      return true;
    case TUPLE_UINT:
      *out = tuple->length == 1 ? tuple->value->uint8
           : tuple->length == 2 ? tuple->value->uint16
           : (int32_t)tuple->value->uint32;
      return true;
    default:
      return false;
  }
}

// Data written by 1.x can hold garbage settings and a goal cached at 30x the
// real average; drop the cache so today's goal is recomputed.
static void prv_migrate_storage(void) {
  if (prv_read_int_or(PERSIST_KEY_SCHEMA_VERSION, 1) >= SCHEMA_VERSION) {
    return;
  }
  persist_delete(PERSIST_KEY_CACHED_GOAL);
  persist_delete(PERSIST_KEY_CACHED_GOAL_DAY);
  persist_write_int(PERSIST_KEY_SCHEMA_VERSION, SCHEMA_VERSION);
}

// --- Days -------------------------------------------------------------------

static int prv_day_key(time_t t) {
  struct tm *tm_info = localtime(&t);
  return tm_info->tm_year * 400 + tm_info->tm_yday;
}

// Local midnight `offset` days away from the day that starts at `midnight`.
static time_t prv_shift_midnight(time_t midnight, int offset) {
  // Aim for noon so a DST change can't land us in the neighbouring day.
  const time_t noon = midnight + offset * SECONDS_PER_DAY + 12 * SECONDS_PER_HOUR;
  struct tm *tm_info = localtime(&noon);
  return noon - (tm_info->tm_hour * SECONDS_PER_HOUR + tm_info->tm_min * SECONDS_PER_MINUTE
                 + tm_info->tm_sec);
}

static bool prv_in_rollover_grace(void) {
  return time(NULL) - time_start_of_today() < ROLLOVER_GRACE_S;
}

int game_get_day_number(void) {
  return prv_day_key(time(NULL));
}

// --- Goal -------------------------------------------------------------------

static int32_t prv_round_goal(int32_t steps) {
  const int32_t goal = ((steps + 50) / 100) * 100;
  return goal < MIN_AUTO_GOAL ? MIN_AUTO_GOAL : (goal > MAX_STEP_GOAL ? MAX_STEP_GOAL : goal);
}

// The Health app's baseline: the average full-day total across the week.
static int32_t prv_resolve_auto_goal(void) {
  const time_t start = time_start_of_today();
  const time_t end = start + SECONDS_PER_DAY;
  const HealthServiceAccessibilityMask mask = health_service_metric_averaged_accessible(
      HealthMetricStepCount, start, end, HealthServiceTimeScopeDaily);
  if (mask & HealthServiceAccessibilityMaskAvailable) {
    const HealthValue average = health_service_sum_averaged(
        HealthMetricStepCount, start, end, HealthServiceTimeScopeDaily);
    if (average > 0) {
      return prv_round_goal(average);
    }
  }
  return prv_read_int_or(PERSIST_KEY_CACHED_GOAL, DEFAULT_STEP_GOAL);
}

static bool prv_is_weekend(time_t day_start) {
  // Midday, so a DST change can't tip us into the neighbouring day.
  const time_t midday = day_start + 12 * SECONDS_PER_HOUR;
  struct tm *tm_info = localtime(&midday);
  return tm_info->tm_wday == 0 || tm_info->tm_wday == 6;
}

static int32_t prv_median(int32_t *values, int count) {
  for (int i = 1; i < count; i++) {
    const int32_t value = values[i];
    int j = i - 1;
    while (j >= 0 && values[j] > value) {
      values[j + 1] = values[j];
      j--;
    }
    values[j + 1] = value;
  }
  return count % 2 ? values[count / 2] : (values[count / 2 - 1] + values[count / 2]) / 2;
}

static int prv_smart_level(void) {
  return prv_read_int_or(PERSIST_KEY_SMART_LEVEL, 0);
}

static void prv_nudge_smart_level(int delta) {
  const int level = prv_smart_level() + delta;
  persist_write_int(PERSIST_KEY_SMART_LEVEL, level < SMART_LEVEL_MIN ? SMART_LEVEL_MIN
                                           : (level > SMART_LEVEL_MAX ? SMART_LEVEL_MAX : level));
}

// The goal for the day starting at `day_start`: a little under a typical day
// before it (weekends judged against weekends), asking a bit more after each
// goal hit and easing off after misses.
static int32_t prv_smart_goal(time_t day_start) {
  int32_t same_kind[SMART_HISTORY_DAYS];
  int32_t any_kind[SMART_HISTORY_DAYS];
  int same_count = 0;
  int any_count = 0;
  const bool weekend = prv_is_weekend(day_start);
  for (int offset = 1; offset <= SMART_HISTORY_DAYS; offset++) {
    const time_t start = prv_shift_midnight(day_start, -offset);
    const time_t end = prv_shift_midnight(day_start, 1 - offset);
    if (!(health_service_metric_accessible(HealthMetricStepCount, start, end)
          & HealthServiceAccessibilityMaskAvailable)) {
      continue;
    }
    const int32_t steps = (int32_t)health_service_sum(HealthMetricStepCount, start, end);
    if (steps < SMART_WORN_STEPS) {
      continue;
    }
    any_kind[any_count++] = steps;
    if (prv_is_weekend(start) == weekend) {
      same_kind[same_count++] = steps;
    }
  }

  int32_t typical;
  if (same_count >= SMART_SAME_KIND_DAYS) {
    typical = prv_median(same_kind, same_count);
  } else if (any_count >= SMART_MIN_DAYS) {
    typical = prv_median(any_kind, any_count);
  } else {
    return prv_resolve_auto_goal();
  }
  return prv_round_goal(typical * (90 + 5 * prv_smart_level()) / 100);
}

// Today's goal is fixed once the day has rolled over, so it can't drift as
// the history updates.
static int32_t prv_goal_for_today(bool day_settled) {
  const int today = game_get_day_number();
  if (prv_read_int_or(PERSIST_KEY_CACHED_GOAL_DAY, -1) == today
      && persist_exists(PERSIST_KEY_CACHED_GOAL)) {
    return persist_read_int(PERSIST_KEY_CACHED_GOAL);
  }
  const int32_t goal = s_settings.step_goal > 0 ? s_settings.step_goal
                                                : prv_smart_goal(time_start_of_today());
  if (day_settled) {
    if (persist_exists(PERSIST_KEY_CACHED_GOAL)) {
      persist_write_int(PERSIST_KEY_PREVIOUS_GOAL, persist_read_int(PERSIST_KEY_CACHED_GOAL));
    }
    persist_write_int(PERSIST_KEY_CACHED_GOAL, goal);
    persist_write_int(PERSIST_KEY_CACHED_GOAL_DAY, today);
  }
  return goal;
}

static int8_t prv_goal_trend(int32_t goal) {
  if (s_settings.step_goal > 0 || !persist_exists(PERSIST_KEY_PREVIOUS_GOAL)) {
    return 0;
  }
  const int32_t previous = persist_read_int(PERSIST_KEY_PREVIOUS_GOAL);
  return goal > previous ? 1 : (goal < previous ? -1 : 0);
}

// The goal a finished day was played against. Days the face never showed get
// the goal it would have shown then.
static int32_t prv_goal_for_day(int day, time_t day_start) {
  if (prv_read_int_or(PERSIST_KEY_CACHED_GOAL_DAY, -1) == day
      && persist_exists(PERSIST_KEY_CACHED_GOAL)) {
    return persist_read_int(PERSIST_KEY_CACHED_GOAL);
  }
  return s_settings.step_goal > 0 ? s_settings.step_goal : prv_smart_goal(day_start);
}

static int32_t prv_survival_steps(int32_t goal) {
  return s_settings.difficulty == DIFFICULTY_FORGIVING ? goal / 2 : goal;
}

static void prv_invalidate_goal_cache(void) {
  persist_delete(PERSIST_KEY_CACHED_GOAL_DAY);
}

// --- Cats -------------------------------------------------------------------

static void prv_set_cat_count(int count) {
  s_state.cat_count = count < 0 ? 0 : (count > MAX_CATS ? MAX_CATS : count);
  s_state.is_dead = s_state.cat_count == 0;
  persist_write_int(PERSIST_KEY_CAT_COUNT, s_state.cat_count);
  persist_write_bool(PERSIST_KEY_IS_DEAD, s_state.is_dead);
}

static void prv_revive(int day) {
  prv_set_cat_count(1);
  persist_write_int(PERSIST_KEY_REVIVED_DAY, day);
}

static void prv_reward_day(int day) {
  if (prv_read_int_or(PERSIST_KEY_LAST_REWARD_DAY, -1) >= day || s_state.cat_count >= MAX_CATS) {
    return;
  }
  prv_set_cat_count(s_state.cat_count + 1);
  persist_write_int(PERSIST_KEY_LAST_REWARD_DAY, day);
}

// --- Daily judgment ---------------------------------------------------------

// Steps walked on a finished day, or -1 if nothing is known about it. Health
// keeps daily totals whether or not this face was running; the snapshot only
// covers the part of the day the face saw, so on its own it's a lower bound
// and `*complete` stays false.
static int32_t prv_steps_for_day(int day, time_t start, time_t end, int snapshot_day,
                                 bool *complete) {
  int32_t steps = -1;
  *complete = false;
  if (health_service_metric_accessible(HealthMetricStepCount, start, end)
      & HealthServiceAccessibilityMaskAvailable) {
    steps = (int32_t)health_service_sum(HealthMetricStepCount, start, end);
    *complete = true;
  }
  if (day == snapshot_day && persist_exists(PERSIST_KEY_LAST_DAY_STEPS)) {
    const int32_t snapshot = persist_read_int(PERSIST_KEY_LAST_DAY_STEPS);
    if (snapshot > steps) {
      steps = snapshot;
    }
  }
  return steps;
}

// A partial count can revive or reward, but only a complete one costs a cat.
static void prv_judge_day(int day, time_t day_start, int32_t steps, bool complete) {
  const int32_t goal = prv_goal_for_day(day, day_start);
  if (goal <= 0) {
    return;
  }
  if (s_state.is_dead) {
    if (steps < RESURRECTION_STEPS) {
      return;
    }
    prv_revive(day);
  }
  const bool smart = s_settings.step_goal == 0;
  if (steps >= goal) {
    prv_reward_day(day);
    if (smart) {
      prv_nudge_smart_level(1);
    }
  } else if (complete && steps < prv_survival_steps(goal)
             && prv_read_int_or(PERSIST_KEY_REVIVED_DAY, -1) != day) {
    prv_set_cat_count(s_state.cat_count - 1);
    if (smart) {
      prv_nudge_smart_level(-1);
    }
  }
}

// Judges every finished day since the last evaluation, oldest first.
// Safe to call repeatedly; guarded by the persisted last-evaluated day.
static void prv_on_day_change(void) {
  const int today = game_get_day_number();
  const int last_eval = prv_read_int_or(PERSIST_KEY_LAST_EVAL_DAY, -1);

  s_current_day = today;
  if (last_eval == today) {
    return;
  }

  // Nothing to judge on the very first run.
  if (last_eval >= 0) {
    const time_t today_start = time_start_of_today();
    for (int offset = -MAX_CATCHUP_DAYS; offset < 0; offset++) {
      const time_t start = prv_shift_midnight(today_start, offset);
      const int day = prv_day_key(start);
      if (day < last_eval) {
        continue;
      }
      const time_t end = prv_shift_midnight(today_start, offset + 1);
      bool complete;
      const int32_t steps = prv_steps_for_day(day, start, end, last_eval, &complete);
      if (steps >= 0) {
        prv_judge_day(day, start, steps, complete);
      }
    }
  }

  persist_write_int(PERSIST_KEY_LAST_EVAL_DAY, today);
  persist_write_int(PERSIST_KEY_LAST_DAY_STEPS, 0);
  s_last_persisted_steps = 0;
}

// --- Live state -------------------------------------------------------------

static float prv_get_pace_fraction(void) {
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  const int current_minutes = tm_info->tm_hour * 60 + tm_info->tm_min;
  const int start_minutes = s_settings.active_start * 60;
  const int end_minutes = s_settings.active_end * 60;

  if (current_minutes <= start_minutes) {
    return 0.0f;
  }
  if (current_minutes >= end_minutes) {
    return 1.0f;
  }
  return (float)(current_minutes - start_minutes) / (float)(end_minutes - start_minutes);
}

// The cat's mood follows how the day is going against the steps it needs.
static PetState prv_pace_state(void) {
  s_state.expected_steps = (int32_t)((float)s_state.survival_steps * prv_get_pace_fraction());
  if (s_state.expected_steps <= 0 || s_state.steps >= s_state.expected_steps) {
    return PET_STATE_ENERGETIC;
  }
  if (s_state.steps * 5 >= s_state.expected_steps * 4) {
    return PET_STATE_LAZY;
  }
  return PET_STATE_DISTRESSED;
}

static PetState prv_resolve_pet_state(void) {
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
  return s_state.pace_state;
}

static bool prv_crossed(int32_t before, int32_t after, int32_t line) {
  return before < line && after >= line;
}

static void prv_update_snapshot(void) {
  const int32_t steps = s_state.steps;
  const int32_t before = s_last_persisted_steps;
  if (steps <= before) {
    return;
  }
  if (steps - before >= SNAPSHOT_STEP_DELTA
      || prv_crossed(before, steps, s_state.survival_steps)
      || prv_crossed(before, steps, s_state.step_goal)) {
    persist_write_int(PERSIST_KEY_LAST_DAY_STEPS, steps);
    s_last_persisted_steps = steps;
  }
}

void game_update(void) {
  const int today = game_get_day_number();
  if (today != s_current_day && !prv_in_rollover_grace()) {
    prv_on_day_change();
  }
  const bool day_settled = today == s_current_day;

  s_state.steps = (int32_t)health_service_sum_today(HealthMetricStepCount);
  s_state.step_goal = prv_goal_for_today(day_settled);
  s_state.goal_trend = prv_goal_trend(s_state.step_goal);
  s_state.survival_steps = prv_survival_steps(s_state.step_goal);

  s_state.is_sleeping = (health_service_peek_current_activities() & HealthActivitySleep) != 0;
  const BatteryChargeState battery = battery_state_service_peek();
  s_state.is_charging = battery.is_plugged || battery.is_charging;
  s_state.is_low_battery = battery.charge_percent < LOW_BATTERY_THRESHOLD;

  if (day_settled) {
    prv_update_snapshot();
    if (s_state.is_dead && s_state.steps >= RESURRECTION_STEPS) {
      prv_revive(today);
    }
    if (!s_state.is_dead && s_state.steps >= s_state.step_goal) {
      prv_reward_day(today);
    }
  }

  if (s_state.is_dead) {
    s_state.resurrection_progress = s_state.steps;
    s_state.progress = (float)s_state.steps / (float)RESURRECTION_STEPS;
  } else {
    s_state.resurrection_progress = 0;
    s_state.progress = s_state.step_goal > 0 ? (float)s_state.steps / (float)s_state.step_goal : 0.0f;
  }
  if (s_state.progress > 1.0f) {
    s_state.progress = 1.0f;
  }

  s_state.pace_state = prv_pace_state();
  s_state.pet_state = prv_resolve_pet_state();
}

void game_init(void) {
  prv_migrate_storage();
  prv_load_settings();
  memset(&s_state, 0, sizeof(s_state));
  s_state.resurrection_goal = RESURRECTION_STEPS;

  int cats = 1;
  if (persist_exists(PERSIST_KEY_CAT_COUNT)) {
    cats = persist_read_int(PERSIST_KEY_CAT_COUNT);
  } else if (persist_exists(PERSIST_KEY_IS_DEAD) && persist_read_bool(PERSIST_KEY_IS_DEAD)) {
    cats = 0;
  }
  prv_set_cat_count(cats);

  s_last_persisted_steps = prv_read_int_or(PERSIST_KEY_LAST_DAY_STEPS, 0);

  // Leave s_current_day at -1 so the first game_update() evaluates any
  // missed midnight rollover (e.g. watchface was not running at midnight).
  s_current_day = -1;
  game_update();
}

void game_deinit(void) {
}

void game_on_settings_received(DictionaryIterator *iter) {
  GameSettings next = s_settings;
  int32_t value;

  if (prv_tuple_int(dict_find(iter, MESSAGE_KEY_StepGoal), &value)) {
    next.step_goal = value;
  }
  if (prv_tuple_int(dict_find(iter, MESSAGE_KEY_CatBreed), &value)) {
    next.cat_breed = (CatBreed)value;
  }
  if (prv_tuple_int(dict_find(iter, MESSAGE_KEY_Difficulty), &value)) {
    next.difficulty = (Difficulty)value;
  }
  if (prv_tuple_int(dict_find(iter, MESSAGE_KEY_ActiveStart), &value)) {
    next.active_start = value;
  }
  if (prv_tuple_int(dict_find(iter, MESSAGE_KEY_ActiveEnd), &value)) {
    next.active_end = value;
  }

  prv_sanitize_settings(&next);
  s_settings = next;
  prv_save_settings();
  prv_invalidate_goal_cache();
  game_update();
}

const GameSettings *game_get_settings(void) {
  return &s_settings;
}

const GameState *game_get_state(void) {
  return &s_state;
}
