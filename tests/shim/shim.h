// Test-side controls for the fake Pebble environment in pebble.h.
#pragma once

#include "pebble.h"

// Wipes storage, health data, battery and clock back to defaults.
void shim_reset(void);

// Local midnight of the given calendar date (TZ=UTC in tests).
time_t shim_day(int year, int month, int day);
// Moves the clock to `day` plus the given wall-clock offset.
void shim_set_now(time_t day, int hour, int minute, int second);

// Full-day step total recorded by Health for the day starting at `day`.
// For today it is the count so far.
void shim_set_day_steps(time_t day, int32_t steps);
// Average daily step total reported by health_service_sum_averaged().
// Zero means no averages are available.
void shim_set_average_daily_steps(int32_t steps);
// Whether Health history can be read at all.
void shim_set_health_available(bool available);
void shim_set_sleeping(bool sleeping);

void shim_set_battery(uint8_t percent, bool charging, bool plugged);

void shim_dict_reset(DictionaryIterator *iter);
void shim_dict_add_cstring(DictionaryIterator *iter, uint32_t key, const char *value);
void shim_dict_add_int32(DictionaryIterator *iter, uint32_t key, int32_t value);
void shim_dict_add_uint8(DictionaryIterator *iter, uint32_t key, uint8_t value);
