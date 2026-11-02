// Host-side tests for the NineLives game rules in src/c/game.c.
// Run with tests/run.sh.
#include "shim.h"
#include "game.h"

static int s_checks;
static int s_failures;
static int s_test_failures;

#define CHECK(cond)                                                        \
  do {                                                                     \
    s_checks++;                                                            \
    if (!(cond)) {                                                         \
      s_failures++;                                                        \
      s_test_failures++;                                                   \
      printf("    %s:%d: expected %s\n", __FILE__, __LINE__, #cond);       \
    }                                                                      \
  } while (0)

#define CHECK_EQ(actual, expected)                                         \
  do {                                                                     \
    const long long actual_ = (long long)(actual);                         \
    const long long expected_ = (long long)(expected);                     \
    s_checks++;                                                            \
    if (actual_ != expected_) {                                            \
      s_failures++;                                                        \
      s_test_failures++;                                                   \
      printf("    %s:%d: %s is %lld, expected %lld\n", __FILE__, __LINE__, \
             #actual, actual_, expected_);                                 \
    }                                                                      \
  } while (0)

static const time_t *prv_days(void) {
  static time_t days[6];
  for (int i = 0; i < 6; i++) {
    days[i] = shim_day(2026, 6, 1 + i);
  }
  return days;
}

static void prv_use_fixed_goal(int32_t goal) {
  persist_write_int(PERSIST_KEY_STEP_GOAL, goal);
}

// --- Settings -------------------------------------------------------------

// Clay sends select and input values as strings.
static void test_settings_sent_as_strings_are_parsed(void) {
  game_init();

  DictionaryIterator iter;
  shim_dict_reset(&iter);
  shim_dict_add_cstring(&iter, MESSAGE_KEY_StepGoal, "8000");
  shim_dict_add_cstring(&iter, MESSAGE_KEY_CatBreed, "2");
  shim_dict_add_cstring(&iter, MESSAGE_KEY_Difficulty, "0");
  shim_dict_add_cstring(&iter, MESSAGE_KEY_ActiveStart, "9");
  shim_dict_add_cstring(&iter, MESSAGE_KEY_ActiveEnd, "21");
  game_on_settings_received(&iter);

  const GameSettings *settings = game_get_settings();
  CHECK_EQ(settings->step_goal, 8000);
  CHECK_EQ(settings->cat_breed, CAT_BREED_SIAMESE);
  CHECK_EQ(settings->difficulty, DIFFICULTY_FORGIVING);
  CHECK_EQ(settings->active_start, 9);
  CHECK_EQ(settings->active_end, 21);
  CHECK_EQ(game_get_state()->step_goal, 8000);
}

static void test_settings_sent_as_integers_are_parsed(void) {
  game_init();

  DictionaryIterator iter;
  shim_dict_reset(&iter);
  shim_dict_add_int32(&iter, MESSAGE_KEY_StepGoal, 6500);
  shim_dict_add_uint8(&iter, MESSAGE_KEY_CatBreed, 3);
  shim_dict_add_uint8(&iter, MESSAGE_KEY_Difficulty, 0);
  game_on_settings_received(&iter);

  const GameSettings *settings = game_get_settings();
  CHECK_EQ(settings->step_goal, 6500);
  CHECK_EQ(settings->cat_breed, CAT_BREED_VOID);
  CHECK_EQ(settings->difficulty, DIFFICULTY_FORGIVING);
}

// Version 1.x stored the raw string bytes of the goal as a number.
static void test_garbage_goal_saved_by_old_version_falls_back_to_auto(void) {
  persist_write_int(PERSIST_KEY_STEP_GOAL, 808464440);
  shim_set_average_daily_steps(7300);
  game_init();
  CHECK_EQ(game_get_settings()->step_goal, 0);
  CHECK_EQ(game_get_state()->step_goal, 7300);
}

// --- Goal -----------------------------------------------------------------

static void test_auto_goal_is_the_average_daily_total(void) {
  shim_set_average_daily_steps(7300);
  game_init();
  CHECK_EQ(game_get_state()->step_goal, 7300);
}

// Version 1.x cached 30x the average as today's goal.
static void test_goal_cached_by_old_version_is_recomputed(void) {
  const time_t *day = prv_days();
  shim_set_now(day[0], 12, 0, 0);
  persist_write_int(PERSIST_KEY_CACHED_GOAL, 219000);
  persist_write_int(PERSIST_KEY_CACHED_GOAL_DAY, game_get_day_number());
  shim_set_average_daily_steps(7300);
  game_init();
  CHECK_EQ(game_get_state()->step_goal, 7300);
}

// --- Daily judgment -------------------------------------------------------

static void test_first_run_starts_with_one_cat(void) {
  const time_t *day = prv_days();
  shim_set_now(day[0], 12, 0, 0);
  game_init();
  CHECK_EQ(game_get_state()->cat_count, 1);
  CHECK(!game_get_state()->is_dead);
}

// Steps walked while another app or face was open still count.
static void test_finished_day_is_judged_on_its_health_total(void) {
  const time_t *day = prv_days();
  prv_use_fixed_goal(8000);
  shim_set_now(day[0], 10, 0, 0);
  shim_set_day_steps(day[0], 2000);
  game_init();

  shim_set_day_steps(day[0], 9000);
  shim_set_day_steps(day[1], 300);
  shim_set_now(day[1], 8, 0, 0);
  game_init();

  CHECK(!game_get_state()->is_dead);
  CHECK_EQ(game_get_state()->cat_count, 2);
}

static void test_each_failed_day_while_away_costs_a_cat(void) {
  const time_t *day = prv_days();
  prv_use_fixed_goal(8000);
  persist_write_int(PERSIST_KEY_CAT_COUNT, 3);
  shim_set_now(day[0], 23, 0, 0);
  shim_set_day_steps(day[0], 9000);
  game_init();

  shim_set_day_steps(day[1], 500);
  shim_set_day_steps(day[2], 500);
  shim_set_day_steps(day[3], 100);
  shim_set_now(day[3], 9, 0, 0);
  game_init();

  CHECK_EQ(game_get_state()->cat_count, 1);
}

static void test_days_away_that_meet_the_goal_earn_cats(void) {
  const time_t *day = prv_days();
  prv_use_fixed_goal(8000);
  shim_set_now(day[0], 9, 0, 0);
  shim_set_day_steps(day[0], 1000);
  game_init();

  shim_set_day_steps(day[0], 9000);
  shim_set_day_steps(day[1], 9000);
  shim_set_day_steps(day[2], 100);
  shim_set_now(day[2], 9, 0, 0);
  game_init();

  CHECK_EQ(game_get_state()->cat_count, 3);
}

// Around midnight Health can briefly report yesterday's total as today's.
static void test_stale_steps_right_after_midnight_do_not_earn_a_cat(void) {
  const time_t *day = prv_days();
  prv_use_fixed_goal(8000);
  shim_set_now(day[0], 23, 59, 0);
  shim_set_day_steps(day[0], 9000);
  game_init();
  CHECK_EQ(game_get_state()->cat_count, 2);

  shim_set_now(day[1], 0, 0, 30);
  shim_set_day_steps(day[1], 9000);
  game_update();
  CHECK_EQ(game_get_state()->cat_count, 2);

  shim_set_now(day[1], 0, 2, 0);
  shim_set_day_steps(day[1], 0);
  game_update();
  CHECK_EQ(game_get_state()->cat_count, 2);
}

// A cat brought back mid-day is not judged on the day it spent dead.
static void test_revival_day_is_not_held_against_the_cat(void) {
  const time_t *day = prv_days();
  prv_use_fixed_goal(8000);
  persist_write_int(PERSIST_KEY_CAT_COUNT, 0);
  shim_set_now(day[0], 8, 0, 0);
  shim_set_day_steps(day[0], 200);
  game_init();
  CHECK(game_get_state()->is_dead);

  shim_set_now(day[0], 10, 0, 0);
  shim_set_day_steps(day[0], 1200);
  game_update();
  CHECK_EQ(game_get_state()->cat_count, 1);

  shim_set_day_steps(day[0], 3000);
  shim_set_day_steps(day[1], 100);
  shim_set_now(day[1], 9, 0, 0);
  game_init();
  CHECK(!game_get_state()->is_dead);
  CHECK_EQ(game_get_state()->cat_count, 1);
}

// Without Health's record of a day, the face's own snapshot is only a lower
// bound: it can save a cat but never cost one.
static void test_partial_snapshot_alone_never_costs_a_cat(void) {
  const time_t *day = prv_days();
  prv_use_fixed_goal(8000);
  shim_set_now(day[0], 10, 0, 0);
  shim_set_day_steps(day[0], 2000);
  game_init();

  shim_set_health_available(false);
  shim_set_now(day[1], 9, 0, 0);
  game_init();
  CHECK(!game_get_state()->is_dead);
  CHECK_EQ(game_get_state()->cat_count, 1);
}

static void test_day_away_with_enough_steps_revives_and_rewards(void) {
  const time_t *day = prv_days();
  prv_use_fixed_goal(8000);
  persist_write_int(PERSIST_KEY_CAT_COUNT, 0);
  shim_set_now(day[0], 8, 0, 0);
  shim_set_day_steps(day[0], 200);
  game_init();
  CHECK(game_get_state()->is_dead);

  shim_set_day_steps(day[0], 9000);
  shim_set_now(day[1], 9, 0, 0);
  game_init();
  CHECK_EQ(game_get_state()->cat_count, 2);
}

// Only the last week counts after a long absence.
static void test_days_older_than_a_week_are_not_judged(void) {
  persist_write_int(PERSIST_KEY_STEP_GOAL, 8000);
  persist_write_int(PERSIST_KEY_DIFFICULTY, DIFFICULTY_FORGIVING);
  persist_write_int(PERSIST_KEY_CAT_COUNT, 3);
  shim_set_now(shim_day(2026, 6, 1), 12, 0, 0);
  game_init();

  // June 1-4 would cost every cat; June 5-11 are neither good nor bad.
  for (int d = 5; d <= 11; d++) {
    shim_set_day_steps(shim_day(2026, 6, d), 6000);
  }
  shim_set_now(shim_day(2026, 6, 12), 9, 0, 0);
  game_init();
  CHECK_EQ(game_get_state()->cat_count, 3);
}

static void test_new_year_rollover_judges_new_years_eve(void) {
  prv_use_fixed_goal(8000);
  const time_t eve = shim_day(2026, 12, 31);
  const time_t new_year = shim_day(2027, 1, 1);
  shim_set_now(eve, 10, 0, 0);
  shim_set_day_steps(eve, 1000);
  game_init();

  shim_set_day_steps(eve, 9000);
  shim_set_now(new_year, 9, 0, 0);
  game_init();
  CHECK_EQ(game_get_state()->cat_count, 2);
}

static void test_out_of_range_settings_fall_back_to_defaults(void) {
  game_init();

  DictionaryIterator iter;
  shim_dict_reset(&iter);
  shim_dict_add_cstring(&iter, MESSAGE_KEY_StepGoal, "250000");
  shim_dict_add_cstring(&iter, MESSAGE_KEY_CatBreed, "7");
  shim_dict_add_cstring(&iter, MESSAGE_KEY_Difficulty, "5");
  shim_dict_add_cstring(&iter, MESSAGE_KEY_ActiveStart, "22");
  shim_dict_add_cstring(&iter, MESSAGE_KEY_ActiveEnd, "8");
  game_on_settings_received(&iter);

  const GameSettings *settings = game_get_settings();
  CHECK_EQ(settings->step_goal, 0);
  CHECK_EQ(settings->cat_breed, CAT_BREED_ORANGE);
  CHECK_EQ(settings->difficulty, DIFFICULTY_STRICT);
  CHECK_EQ(settings->active_start, DEFAULT_ACTIVE_START);
  CHECK_EQ(settings->active_end, DEFAULT_ACTIVE_END);
}

// --- Smart goal ------------------------------------------------------------

// Gives each of the `count` days before `today` the same step total.
static void prv_fill_days(time_t today, int count, int32_t steps) {
  for (int i = 1; i <= count; i++) {
    shim_set_day_steps(today - i * SECONDS_PER_DAY, steps);
  }
}

// June 15, 2026 is a Monday.
static void test_smart_goal_starts_a_little_under_your_typical_day(void) {
  const time_t monday = shim_day(2026, 6, 15);
  prv_fill_days(monday, 28, 8000);
  shim_set_now(monday, 12, 0, 0);
  game_init();
  CHECK_EQ(game_get_state()->step_goal, 7200);
}

static void test_smart_goal_ignores_days_without_the_watch(void) {
  const time_t monday = shim_day(2026, 6, 15);
  for (int i = 1; i <= 28; i++) {
    shim_set_day_steps(monday - i * SECONDS_PER_DAY, i % 2 ? 6000 : 150);
  }
  shim_set_now(monday, 12, 0, 0);
  game_init();
  CHECK_EQ(game_get_state()->step_goal, 5400);
}

static void prv_fill_weekdays_and_weekends(time_t today, int32_t weekday, int32_t weekend) {
  for (int i = 1; i <= 28; i++) {
    const time_t day = today - i * SECONDS_PER_DAY;
    struct tm *tm_info = gmtime(&day);
    const bool is_weekend = tm_info->tm_wday == 0 || tm_info->tm_wday == 6;
    shim_set_day_steps(day, is_weekend ? weekend : weekday);
  }
}

static void test_smart_goal_judges_weekends_against_weekends(void) {
  const time_t saturday = shim_day(2026, 6, 13);
  prv_fill_weekdays_and_weekends(saturday, 10000, 4000);
  shim_set_now(saturday, 12, 0, 0);
  game_init();
  CHECK_EQ(game_get_state()->step_goal, 3600);

  shim_reset();
  const time_t monday = shim_day(2026, 6, 15);
  prv_fill_weekdays_and_weekends(monday, 10000, 4000);
  shim_set_now(monday, 12, 0, 0);
  game_init();
  CHECK_EQ(game_get_state()->step_goal, 9000);
}

static void test_smart_goal_rises_after_a_day_you_hit_it(void) {
  const time_t monday = shim_day(2026, 6, 15);
  prv_fill_days(monday, 28, 8000);
  shim_set_now(monday, 8, 0, 0);
  game_init();
  CHECK_EQ(game_get_state()->step_goal, 7200);

  shim_set_day_steps(monday, 7500);
  shim_set_now(monday + SECONDS_PER_DAY, 9, 0, 0);
  game_init();
  CHECK_EQ(game_get_state()->step_goal, 7600);
  CHECK_EQ(game_get_state()->goal_trend, 1);
}

static void test_smart_goal_eases_after_a_miss(void) {
  const time_t monday = shim_day(2026, 6, 15);
  prv_fill_days(monday, 28, 8000);
  shim_set_now(monday, 8, 0, 0);
  game_init();

  shim_set_day_steps(monday, 3000);
  shim_set_now(monday + SECONDS_PER_DAY, 9, 0, 0);
  game_init();
  CHECK_EQ(game_get_state()->step_goal, 6800);
  CHECK_EQ(game_get_state()->goal_trend, -1);
}

// With steady days the goal settles where you actually walk.
static void test_smart_goal_settles_on_steady_days(void) {
  const time_t monday = shim_day(2026, 6, 15);
  prv_fill_days(monday, 28, 9000);
  shim_set_now(shim_day(2026, 6, 1), 12, 0, 0);
  game_init();

  shim_set_now(monday, 9, 0, 0);
  game_init();
  CHECK(game_get_state()->step_goal >= 8500);
  CHECK(game_get_state()->step_goal <= 9500);
}

// A big week can't push the goal past 110% of a typical day.
static void test_smart_goal_stops_rising_at_ten_percent_over(void) {
  const time_t monday = shim_day(2026, 6, 15);
  prv_fill_days(monday, 28, 9000);
  prv_fill_days(monday, 7, 20000);
  shim_set_now(shim_day(2026, 6, 1), 12, 0, 0);
  game_init();

  shim_set_now(monday, 9, 0, 0);
  game_init();
  CHECK_EQ(game_get_state()->step_goal, 9900);
}

static void test_fixed_goal_never_moves(void) {
  prv_use_fixed_goal(8000);
  const time_t start = shim_day(2026, 6, 1);
  shim_set_now(start, 12, 0, 0);
  game_init();

  const time_t monday = shim_day(2026, 6, 15);
  prv_fill_days(monday, 28, 12000);
  shim_set_now(monday, 9, 0, 0);
  game_init();
  CHECK_EQ(game_get_state()->step_goal, 8000);
  CHECK_EQ(game_get_state()->goal_trend, 0);
}

// --- Mood -----------------------------------------------------------------

// On Forgiving, half the goal keeps the cat alive, so pace is measured
// against that.
static void test_forgiving_mood_tracks_half_the_goal(void) {
  const time_t *day = prv_days();
  prv_use_fixed_goal(10000);
  persist_write_int(PERSIST_KEY_DIFFICULTY, DIFFICULTY_FORGIVING);
  shim_set_now(day[0], 15, 0, 0);
  shim_set_day_steps(day[0], 3000);
  game_init();
  CHECK_EQ(game_get_state()->pet_state, PET_STATE_ENERGETIC);
}

// The step bar keeps showing pace while the cat is busy charging or asleep.
static void test_pace_is_tracked_while_charging(void) {
  const time_t *day = prv_days();
  prv_use_fixed_goal(8000);
  shim_set_now(day[0], 15, 0, 0);
  shim_set_day_steps(day[0], 1000);
  shim_set_battery(50, true, true);
  game_init();
  CHECK_EQ(game_get_state()->pet_state, PET_STATE_CHARGING);
  CHECK_EQ(game_get_state()->pace_state, PET_STATE_DISTRESSED);
}

typedef struct {
  const char *name;
  void (*fn)(void);
} TestCase;

#define TEST_CASE(fn) { #fn, fn }

static const TestCase TESTS[] = {
  TEST_CASE(test_settings_sent_as_strings_are_parsed),
  TEST_CASE(test_settings_sent_as_integers_are_parsed),
  TEST_CASE(test_garbage_goal_saved_by_old_version_falls_back_to_auto),
  TEST_CASE(test_auto_goal_is_the_average_daily_total),
  TEST_CASE(test_goal_cached_by_old_version_is_recomputed),
  TEST_CASE(test_first_run_starts_with_one_cat),
  TEST_CASE(test_finished_day_is_judged_on_its_health_total),
  TEST_CASE(test_each_failed_day_while_away_costs_a_cat),
  TEST_CASE(test_days_away_that_meet_the_goal_earn_cats),
  TEST_CASE(test_stale_steps_right_after_midnight_do_not_earn_a_cat),
  TEST_CASE(test_revival_day_is_not_held_against_the_cat),
  TEST_CASE(test_partial_snapshot_alone_never_costs_a_cat),
  TEST_CASE(test_day_away_with_enough_steps_revives_and_rewards),
  TEST_CASE(test_days_older_than_a_week_are_not_judged),
  TEST_CASE(test_new_year_rollover_judges_new_years_eve),
  TEST_CASE(test_out_of_range_settings_fall_back_to_defaults),
  TEST_CASE(test_smart_goal_starts_a_little_under_your_typical_day),
  TEST_CASE(test_smart_goal_ignores_days_without_the_watch),
  TEST_CASE(test_smart_goal_judges_weekends_against_weekends),
  TEST_CASE(test_smart_goal_rises_after_a_day_you_hit_it),
  TEST_CASE(test_smart_goal_eases_after_a_miss),
  TEST_CASE(test_smart_goal_settles_on_steady_days),
  TEST_CASE(test_smart_goal_stops_rising_at_ten_percent_over),
  TEST_CASE(test_fixed_goal_never_moves),
  TEST_CASE(test_forgiving_mood_tracks_half_the_goal),
  TEST_CASE(test_pace_is_tracked_while_charging),
};

int main(void) {
  setenv("TZ", "UTC", 1);
  tzset();

  int failed_tests = 0;
  const int count = (int)(sizeof(TESTS) / sizeof(TESTS[0]));
  for (int i = 0; i < count; i++) {
    shim_reset();
    s_test_failures = 0;
    TESTS[i].fn();
    printf("%s %s\n", s_test_failures ? "FAIL" : "ok  ", TESTS[i].name);
    if (s_test_failures) {
      failed_tests++;
    }
  }
  printf("\n%d tests, %d checks, %d failed tests\n", count, s_checks, failed_tests);
  return failed_tests ? 1 : 0;
}
