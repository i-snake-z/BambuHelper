#pragma once
#include <Arduino.h>

// Runtime weather data (not persisted — fetched from Open-Meteo)
struct WeatherData {
  bool     valid;
  float    temp;          // current temperature
  float    feelsLike;     // apparent temperature
  float    humidity;      // relative humidity %
  float    windSpeed;     // wind speed (km/h or mph depending on settings)
  uint8_t  weatherCode;   // WMO weather interpretation code
  uint32_t lastFetchMs;   // millis() of last successful fetch
};

extern WeatherData weatherData;

// Convert WMO weather code to a short human-readable string
const char* wmoConditionString(uint8_t code);

// Fetch weather if enough time has elapsed since last fetch
// (blocking HTTP call, ~0.5-2s; returns immediately if not due)
void fetchWeatherIfNeeded();

// Force next call to fetchWeatherIfNeeded() to actually fetch
void invalidateWeather();
