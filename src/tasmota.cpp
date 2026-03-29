#include "tasmota.h"
#include "settings.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

#define TASMOTA_TIMEOUT_MS  3000      // HTTP connect+read timeout
#define TASMOTA_STALE_MS    90000UL   // consider offline after 90s without data

static float    g_watts         = -1.0f;
static float    g_totalKwh      = -1.0f;  // cumulative kWh from Tasmota Total field
static float    g_printStartKwh = -1.0f;  // Total at print start
static uint32_t g_lastUpdateMs  = 0;
static uint32_t g_nextPollMs    = 0;
static bool     g_kwhChanged    = false;

void tasmotaInit() {
  g_watts         = -1.0f;
  g_totalKwh      = -1.0f;
  g_printStartKwh = -1.0f;
  g_lastUpdateMs  = 0;
  g_nextPollMs    = 0;  // poll immediately on next loop
  g_kwhChanged    = false;
}

static void doPoll() {
  if (!tasmotaSettings.enabled || tasmotaSettings.ip[0] == '\0') return;

  char url[64];
  snprintf(url, sizeof(url), "http://%s/cm?cmnd=Status%%2010", tasmotaSettings.ip);

  HTTPClient http;
  http.setTimeout(TASMOTA_TIMEOUT_MS);
  if (!http.begin(url)) {
    Serial.printf("[Tasmota] begin failed: %s\n", url);
    return;
  }

  int code = http.GET();
  if (code != 200) {
    Serial.printf("[Tasmota] HTTP %d from %s\n", code, tasmotaSettings.ip);
    http.end();
    return;
  }

  String body = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[Tasmota] JSON parse error: %s\n", err.c_str());
    return;
  }

  // Try StatusSNS path (Status 10) and direct ENERGY path (Status 8)
  JsonVariant energy = doc["StatusSNS"]["ENERGY"];
  if (energy.isNull()) energy = doc["ENERGY"];
  if (energy.isNull()) {
    Serial.println("[Tasmota] ENERGY object missing in response");
    return;
  }

  JsonVariant power = energy["Power"];
  JsonVariant total = energy["Total"];

  if (power.isNull()) {
    Serial.println("[Tasmota] Power field missing");
    return;
  }

  g_watts        = power.as<float>();
  g_lastUpdateMs = millis();

  if (!total.isNull()) {
    float newTotal = total.as<float>();
    if (newTotal != g_totalKwh) {
      g_totalKwh   = newTotal;
      g_kwhChanged = true;
    }
  }

  Serial.printf("[Tasmota] Power=%.0fW, Total=%.3fkWh\n", g_watts, g_totalKwh);
}

void tasmotaLoop(unsigned long now) {
  if (!tasmotaSettings.enabled) return;
  if ((uint32_t)now >= g_nextPollMs) {
    doPoll();
    uint32_t intervalMs = (uint32_t)tasmotaSettings.pollInterval * 1000UL;
    g_nextPollMs = (uint32_t)now + intervalMs;
  }
}

float tasmotaGetWatts() {
  return g_watts;
}

bool tasmotaIsOnline() {
  if (!tasmotaSettings.enabled) return false;
  if (g_lastUpdateMs == 0) return false;
  return (millis() - g_lastUpdateMs) < TASMOTA_STALE_MS;
}

bool tasmotaIsActiveForSlot(uint8_t slot) {
  if (!tasmotaIsOnline()) return false;
  return (tasmotaSettings.assignedSlot == 255 || tasmotaSettings.assignedSlot == slot);
}

void tasmotaMarkPrintStart() {
  g_printStartKwh = g_totalKwh;  // -1 if Tasmota not yet polled
  Serial.printf("[Tasmota] Print start marked, Total=%.3fkWh\n", g_printStartKwh);
}

float tasmotaGetPrintKwhUsed() {
  if (g_printStartKwh < 0.0f || g_totalKwh < 0.0f) return -1.0f;
  float used = g_totalKwh - g_printStartKwh;
  return (used >= 0.0f) ? used : -1.0f;
}

bool tasmotaKwhChanged() {
  if (!g_kwhChanged) return false;
  g_kwhChanged = false;
  return true;
}
