// Host-side stand-in for the parts of the Pebble SDK that game.c uses, so the
// game rules can be compiled and tested with a desktop C compiler.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SECONDS_PER_MINUTE 60
#define SECONDS_PER_HOUR 3600
#define SECONDS_PER_DAY 86400

#define PBL_DISPLAY_WIDTH 144
#define PBL_DISPLAY_HEIGHT 168
#define PBL_IF_ROUND_ELSE(if_true, if_false) (if_false)
#define PBL_IF_COLOR_ELSE(if_true, if_false) (if_true)

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

typedef int32_t status_t;
#define S_SUCCESS 0

#define APP_LOG_LEVEL_ERROR 1
#define APP_LOG_LEVEL_WARNING 50
#define APP_LOG_LEVEL_INFO 100
#define APP_LOG_LEVEL_DEBUG 200
#define APP_LOG(level, fmt, ...) ((void)0)

// Clock. The shim owns "now"; localtime() is the host's, run with TZ=UTC.
time_t shim_time(time_t *out);
#define time(out) shim_time(out)
time_t time_start_of_today(void);

// Persistent storage.
bool persist_exists(const uint32_t key);
bool persist_read_bool(const uint32_t key);
int32_t persist_read_int(const uint32_t key);
status_t persist_write_bool(const uint32_t key, const bool value);
status_t persist_write_int(const uint32_t key, const int32_t value);
status_t persist_delete(const uint32_t key);

// Battery.
typedef struct {
  uint8_t charge_percent;
  bool is_charging;
  bool is_plugged;
} BatteryChargeState;
BatteryChargeState battery_state_service_peek(void);

// Health.
typedef enum {
  HealthMetricStepCount,
  HealthMetricActiveSeconds,
  HealthMetricWalkedDistanceMeters,
  HealthMetricSleepSeconds,
} HealthMetric;

typedef int32_t HealthValue;

typedef enum {
  HealthServiceTimeScopeOnce,
  HealthServiceTimeScopeWeekly,
  HealthServiceTimeScopeDailyWeekdayOrWeekend,
  HealthServiceTimeScopeDaily,
} HealthServiceTimeScope;

typedef enum {
  HealthServiceAccessibilityMaskAvailable = 1 << 0,
  HealthServiceAccessibilityMaskNoPermission = 1 << 1,
  HealthServiceAccessibilityMaskNotSupported = 1 << 2,
  HealthServiceAccessibilityMaskNotAvailable = 1 << 3,
} HealthServiceAccessibilityMask;

typedef enum {
  HealthActivityNone = 0,
  HealthActivitySleep = 1 << 0,
  HealthActivityRestfulSleep = 1 << 1,
  HealthActivityWalk = 1 << 2,
  HealthActivityRun = 1 << 3,
  HealthActivityOpenWorkout = 1 << 4,
} HealthActivity;
typedef uint32_t HealthActivityMask;

HealthValue health_service_sum(HealthMetric metric, time_t time_start, time_t time_end);
HealthValue health_service_sum_today(HealthMetric metric);
HealthValue health_service_sum_averaged(HealthMetric metric, time_t time_start, time_t time_end,
                                        HealthServiceTimeScope scope);
HealthServiceAccessibilityMask health_service_metric_accessible(HealthMetric metric,
                                                                time_t time_start,
                                                                time_t time_end);
HealthServiceAccessibilityMask health_service_metric_averaged_accessible(
    HealthMetric metric, time_t time_start, time_t time_end, HealthServiceTimeScope scope);
HealthActivityMask health_service_peek_current_activities(void);

// AppMessage dictionaries. Field names match the SDK so `tuple->value->int32`
// and `tuple->value->cstring` read the same bytes they would on the watch.
typedef enum {
  TUPLE_BYTE_ARRAY = 0,
  TUPLE_CSTRING = 1,
  TUPLE_UINT = 2,
  TUPLE_INT = 3,
} TupleType;

typedef union {
  uint8_t data[32];
  char cstring[32];
  uint8_t uint8;
  uint16_t uint16;
  uint32_t uint32;
  int8_t int8;
  int16_t int16;
  int32_t int32;
} ShimTupleValue;

typedef struct {
  uint32_t key;
  TupleType type;
  uint16_t length;
  ShimTupleValue value[1];
} Tuple;

typedef struct {
  Tuple tuples[16];
  int count;
} DictionaryIterator;

Tuple *dict_find(const DictionaryIterator *iter, const uint32_t key);

// Normally generated from package.json "messageKeys".
enum {
  MESSAGE_KEY_StepGoal = 10000,
  MESSAGE_KEY_CatBreed,
  MESSAGE_KEY_Difficulty,
  MESSAGE_KEY_ActiveStart,
  MESSAGE_KEY_ActiveEnd,
};
