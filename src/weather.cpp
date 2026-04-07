#include "weather.h"
#include "settings.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <WiFi.h>

WeatherData weatherData = { false, true, 0, 0, 0, 0, 0, 0 };

// ---------------------------------------------------------------------------
//  WMO weather interpretation codes → short condition string
// ---------------------------------------------------------------------------
const char* wmoConditionString(uint8_t code) {
  if (code == 0)           return "Clear sky";
  if (code <= 2)           return code == 1 ? "Mainly clear" : "Partly cloudy";
  if (code == 3)           return "Overcast";
  if (code == 45 || code == 48) return "Fog";
  if (code >= 51 && code <= 55) return "Drizzle";
  if (code >= 56 && code <= 57) return "Freezing drizzle";
  if (code >= 61 && code <= 65) return "Rain";
  if (code >= 66 && code <= 67) return "Freezing rain";
  if (code >= 71 && code <= 77) return "Snow";
  if (code >= 80 && code <= 82) return "Rain showers";
  if (code >= 85 && code <= 86) return "Snow showers";
  if (code == 95)          return "Thunderstorm";
  if (code >= 96)          return "Thunderstorm+hail";
  return "Unknown";
}

void invalidateWeather() {
  weatherData.valid       = false;
  weatherData.lastFetchMs = 0;
}

// ---------------------------------------------------------------------------
//  Fetch from Open-Meteo (HTTP, no API key required)
// ---------------------------------------------------------------------------
void fetchWeatherIfNeeded() {
  if (!weatherSettings.enabled) return;
  if (weatherSettings.lat == 0.0f && weatherSettings.lon == 0.0f) return;
  if (WiFi.status() != WL_CONNECTED) return;

  uint32_t updateMs = (uint32_t)weatherSettings.updateMins * 60000UL;
  if (weatherData.valid && (millis() - weatherData.lastFetchMs) < updateMs) return;

  char url[300];
  snprintf(url, sizeof(url),
    "http://api.open-meteo.com/v1/forecast"
    "?latitude=%.4f&longitude=%.4f"
    "&current=temperature_2m,apparent_temperature,relative_humidity_2m"
    ",wind_speed_10m,weather_code,is_day"
    "&temperature_unit=%s&wind_speed_unit=%s&timezone=auto",
    weatherSettings.lat,
    weatherSettings.lon,
    weatherSettings.useMetric ? "celsius"    : "fahrenheit",
    weatherSettings.useMetric ? "kmh"        : "mph"
  );

  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(8000);

  if (!http.begin(url)) {
    http.end();
    return;
  }

  int httpCode = http.GET();
  if (httpCode != 200) {
    http.end();
    return;
  }

  String body = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) return;

  JsonObject cur = doc["current"];
  if (cur.isNull()) return;

  weatherData.temp        = cur["temperature_2m"].as<float>();
  weatherData.feelsLike   = cur["apparent_temperature"].as<float>();
  weatherData.humidity    = cur["relative_humidity_2m"].as<float>();
  weatherData.windSpeed   = cur["wind_speed_10m"].as<float>();
  weatherData.weatherCode = cur["weather_code"].as<uint8_t>();
  weatherData.isDay       = cur["is_day"].as<int>() != 0;
  weatherData.valid       = true;
  weatherData.lastFetchMs = millis();
}
