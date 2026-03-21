#ifndef TIMEZONES_H
#define TIMEZONES_H

#include <Arduino.h>

struct TimezoneRegion {
  const char* name;           // Display name (e.g., "Central European (Poland, Germany)")
  const char* posixString;    // POSIX TZ string for configTzTime()
};

// Get list of all supported timezones and count
const TimezoneRegion* getSupportedTimezones(size_t* count);

// Get default POSIX TZ string for old gmtOffsetMin value (migration)
const char* getDefaultTimezoneForOffset(int gmtOffsetMinutes);

#endif // TIMEZONES_H
