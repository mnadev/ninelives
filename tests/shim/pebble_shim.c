// Fake Pebble environment. The Health model follows the firmware's
// applib/health_service.c: 30 days of daily totals, with partial first and
// last days of a range scaled by how much of each day the range covers.
#include "shim.h"

#define PERSIST_SLOTS 64
#define HISTORY_DAYS 30
#define MAX_RECORDED_DAYS 64

typedef struct {
  bool exists;
  int32_t value;
} PersistSlot;

typedef struct {
  time_t day;
  int32_t steps;
} DayRecord;

static PersistSlot s_persist[PERSIST_SLOTS];
static DayRecord s_days[MAX_RECORDED_DAYS];
static int s_day_count;
static time_t s_now;
static int32_t s_average_daily_steps;
static bool s_health_available;
static bool s_sleeping;
static BatteryChargeState s_battery;

static time_t prv_midnight(time_t t) {
  struct tm *lt = localtime(&t);
  return t - (lt->tm_hour * SECONDS_PER_HOUR + lt->tm_min * SECONDS_PER_MINUTE + lt->tm_sec);
}

static int32_t prv_day_total(time_t day) {
  for (int i = 0; i < s_day_count; i++) {
    if (s_days[i].day == day) {
      return s_days[i].steps;
    }
  }
  return 0;
}

void shim_reset(void) {
  memset(s_persist, 0, sizeof(s_persist));
  memset(s_days, 0, sizeof(s_days));
  s_day_count = 0;
  s_now = shim_day(2026, 6, 1) + 12 * SECONDS_PER_HOUR;
  s_average_daily_steps = 0;
  s_health_available = true;
  s_sleeping = false;
  s_battery = (BatteryChargeState){ .charge_percent = 80, .is_charging = false, .is_plugged = false };
}

time_t shim_day(int year, int month, int day) {
  struct tm tm_value = { 0 };
  tm_value.tm_year = year - 1900;
  tm_value.tm_mon = month - 1;
  tm_value.tm_mday = day;
  return timegm(&tm_value);
}

void shim_set_now(time_t day, int hour, int minute, int second) {
  s_now = day + hour * SECONDS_PER_HOUR + minute * SECONDS_PER_MINUTE + second;
}

void shim_set_day_steps(time_t day, int32_t steps) {
  for (int i = 0; i < s_day_count; i++) {
    if (s_days[i].day == day) {
      s_days[i].steps = steps;
      return;
    }
  }
  if (s_day_count < MAX_RECORDED_DAYS) {
    s_days[s_day_count++] = (DayRecord){ .day = day, .steps = steps };
  }
}

void shim_set_average_daily_steps(int32_t steps) {
  s_average_daily_steps = steps;
}

void shim_set_health_available(bool available) {
  s_health_available = available;
}

void shim_set_sleeping(bool sleeping) {
  s_sleeping = sleeping;
}

void shim_set_battery(uint8_t percent, bool charging, bool plugged) {
  s_battery = (BatteryChargeState){
    .charge_percent = percent, .is_charging = charging, .is_plugged = plugged
  };
}

time_t shim_time(time_t *out) {
  if (out) {
    *out = s_now;
  }
  return s_now;
}

time_t time_start_of_today(void) {
  return prv_midnight(s_now);
}

bool persist_exists(const uint32_t key) {
  return key < PERSIST_SLOTS && s_persist[key].exists;
}

bool persist_read_bool(const uint32_t key) {
  return persist_read_int(key) != 0;
}

int32_t persist_read_int(const uint32_t key) {
  return persist_exists(key) ? s_persist[key].value : 0;
}

status_t persist_write_bool(const uint32_t key, const bool value) {
  return persist_write_int(key, value ? 1 : 0);
}

status_t persist_write_int(const uint32_t key, const int32_t value) {
  if (key >= PERSIST_SLOTS) {
    return -1;
  }
  s_persist[key] = (PersistSlot){ .exists = true, .value = value };
  return S_SUCCESS;
}

status_t persist_delete(const uint32_t key) {
  if (key < PERSIST_SLOTS) {
    s_persist[key].exists = false;
    s_persist[key].value = 0;
  }
  return S_SUCCESS;
}

BatteryChargeState battery_state_service_peek(void) {
  return s_battery;
}

// Mirrors prv_calculate_time_range(): clamps the range to [history start, now].
static bool prv_clamp_range(time_t *start, time_t *end) {
  const time_t midnight_after_now = prv_midnight(s_now) + SECONDS_PER_DAY;
  *end = MIN(*end, s_now);
  *start = MAX(*start, midnight_after_now - (time_t)SECONDS_PER_DAY * HISTORY_DAYS);
  return *end >= *start;
}

HealthValue health_service_sum(HealthMetric metric, time_t time_start, time_t time_end) {
  if (metric != HealthMetricStepCount || !s_health_available) {
    return 0;
  }
  if (!prv_clamp_range(&time_start, &time_end)) {
    return 0;
  }
  const time_t today = prv_midnight(s_now);
  const time_t midnight_before_start = prv_midnight(time_start);
  const time_t midnight_before_end = prv_midnight(time_end);
  const time_t midnight_after_end = (midnight_before_end == time_end)
      ? midnight_before_end : midnight_before_end + SECONDS_PER_DAY;
  const int num_days = (int)((midnight_after_end - midnight_before_start) / SECONDS_PER_DAY);
  const time_t last_day = midnight_after_end - SECONDS_PER_DAY;

  const int64_t total_seconds = time_end - time_start;
  const int64_t seconds_first_day = SECONDS_PER_DAY - (time_start - midnight_before_start);
  const int64_t seconds_last_day = (time_end == midnight_before_end)
      ? SECONDS_PER_DAY : (time_end - midnight_before_end);
  const int64_t seconds_total_last_day = (last_day == today)
      ? (s_now - midnight_before_end) : SECONDS_PER_DAY;

  int64_t sum = 0;
  for (int i = 0; i < num_days; i++) {
    const time_t day = last_day - (time_t)i * SECONDS_PER_DAY;
    int64_t value = prv_day_total(day);
    if (i == 0) {
      const int64_t covered = (num_days == 1) ? total_seconds : seconds_last_day;
      if (seconds_total_last_day > 0) {
        value = value * covered / seconds_total_last_day;
      }
    } else if (i == num_days - 1) {
      value = value * seconds_first_day / SECONDS_PER_DAY;
    }
    sum += value;
  }
  return (HealthValue)sum;
}

HealthValue health_service_sum_today(HealthMetric metric) {
  if (metric != HealthMetricStepCount || !s_health_available) {
    return 0;
  }
  return prv_day_total(prv_midnight(s_now));
}

// Mirrors prv_compute_aggregate_averaged_using_daily_totals(): the average
// daily total, scaled by how many days the range spans.
HealthValue health_service_sum_averaged(HealthMetric metric, time_t time_start, time_t time_end,
                                        HealthServiceTimeScope scope) {
  (void)scope;
  if (metric != HealthMetricStepCount || !s_health_available || s_average_daily_steps <= 0) {
    return 0;
  }
  return (HealthValue)((int64_t)s_average_daily_steps * (time_end - time_start) / SECONDS_PER_DAY);
}

HealthServiceAccessibilityMask health_service_metric_accessible(HealthMetric metric,
                                                                time_t time_start,
                                                                time_t time_end) {
  if (metric != HealthMetricStepCount || !s_health_available) {
    return HealthServiceAccessibilityMaskNotAvailable;
  }
  return prv_clamp_range(&time_start, &time_end)
      ? HealthServiceAccessibilityMaskAvailable : HealthServiceAccessibilityMaskNotAvailable;
}

HealthServiceAccessibilityMask health_service_metric_averaged_accessible(
    HealthMetric metric, time_t time_start, time_t time_end, HealthServiceTimeScope scope) {
  (void)time_start;
  (void)time_end;
  (void)scope;
  if (metric != HealthMetricStepCount || !s_health_available || s_average_daily_steps <= 0) {
    return HealthServiceAccessibilityMaskNotAvailable;
  }
  return HealthServiceAccessibilityMaskAvailable;
}

HealthActivityMask health_service_peek_current_activities(void) {
  return s_sleeping ? HealthActivitySleep : HealthActivityNone;
}

Tuple *dict_find(const DictionaryIterator *iter, const uint32_t key) {
  for (int i = 0; i < iter->count; i++) {
    if (iter->tuples[i].key == key) {
      return (Tuple *)&iter->tuples[i];
    }
  }
  return NULL;
}

void shim_dict_reset(DictionaryIterator *iter) {
  memset(iter, 0, sizeof(*iter));
}

static Tuple *prv_dict_add(DictionaryIterator *iter, uint32_t key, TupleType type, uint16_t length) {
  Tuple *tuple = &iter->tuples[iter->count++];
  memset(tuple, 0, sizeof(*tuple));
  tuple->key = key;
  tuple->type = type;
  tuple->length = length;
  return tuple;
}

void shim_dict_add_cstring(DictionaryIterator *iter, uint32_t key, const char *value) {
  const size_t length = strlen(value) + 1;
  Tuple *tuple = prv_dict_add(iter, key, TUPLE_CSTRING, (uint16_t)length);
  memcpy(tuple->value->cstring, value, length);
}

void shim_dict_add_int32(DictionaryIterator *iter, uint32_t key, int32_t value) {
  prv_dict_add(iter, key, TUPLE_INT, 4)->value->int32 = value;
}

void shim_dict_add_uint8(DictionaryIterator *iter, uint32_t key, uint8_t value) {
  prv_dict_add(iter, key, TUPLE_UINT, 1)->value->uint8 = value;
}
