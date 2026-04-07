#include "clock_weather.h"
#include "display_ui.h"
#include "settings.h"
#include "weather.h"
#include "config.h"
#include "layout.h"
#include "icons_weather.h"
#include <time.h>

extern TFT_eSPI tft;

// Reuse the same digit geometry as clock_mode.cpp
#define WCK_DIGIT_W   LY_ARK_DIGIT_W
#define WCK_DIGIT_H   LY_ARK_DIGIT_H
#define WCK_COLON_W   LY_ARK_COLON_W
#define WCK_TIME_W    (4 * WCK_DIGIT_W + WCK_COLON_W)
#define WCK_TIME_X    ((LY_W - WCK_TIME_W) / 2)

#if defined(DISPLAY_CYD)
#define WCK_TEXT_CX  74
#else
#define WCK_TEXT_CX  79
#endif

static int  wck_prevMinute  = -1;
static char wck_prevDigits[5] = {0, 0, 0, 0, 0};
static bool wck_prevColon   = false;
static char wck_prevDateBuf[28] = "";
static char wck_prevAmPm[3]    = "";
static char wck_prevCond[48]   = "";
static char wck_prevTemp[16]   = "";
static char wck_prevExtra[48]  = "";
static char wck_prevCity[32]   = "";   // file-scope so resetWeatherClock() can clear it
static bool wck_firstDraw      = true;
static uint8_t wck_prevCode    = 255;  // last drawn icon WMO code

void resetWeatherClock() {
  wck_prevMinute  = -1;
  memset(wck_prevDigits, 0, sizeof(wck_prevDigits));
  wck_prevColon       = false;
  wck_prevDateBuf[0]  = '\0';
  wck_prevAmPm[0]     = '\0';
  wck_prevCond[0]     = '\0';
  wck_prevTemp[0]     = '\0';
  wck_prevExtra[0]    = '\0';
  wck_prevCity[0]     = '\0';
  wck_prevCode        = 255;
  wck_firstDraw       = true;
}

// X position for each digit/colon slot (same formula as clock_mode.cpp)
static int wckDigitX(int i) {
  if (i < 2)  return WCK_TIME_X + i * WCK_DIGIT_W;
  if (i == 2) return WCK_TIME_X + 2 * WCK_DIGIT_W;  // colon slot
  return WCK_TIME_X + 2 * WCK_DIGIT_W + WCK_COLON_W + (i - 3) * WCK_DIGIT_W;
}

// ---------------------------------------------------------------------------
//  Draw a centred single-line text, clearing the old text first.
//  Both oldBuf and newBuf must be null-terminated.
// ---------------------------------------------------------------------------
static void redrawCentredText(const char* oldBuf, const char* newBuf,
                               int centerX, int centerY, uint8_t font,
                               uint16_t color, uint16_t bg) {
  if (strcmp(oldBuf, newBuf) == 0) return;

  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(font);

  // Clear old text
  int oldW = oldBuf[0] ? tft.textWidth(oldBuf) : 0;
  int newW = tft.textWidth(newBuf);
  int clearW = (oldW > newW ? oldW : newW) + 4;
  int clearH = tft.fontHeight(font) + 2;
  tft.fillRect(centerX - clearW / 2, centerY - clearH / 2, clearW, clearH, bg);

  // Draw new text
  tft.setTextColor(color, bg);
  tft.drawString(newBuf, centerX, centerY);
}

// Same as redrawCentredText but uses a GFX (free) font.
static void redrawCentredFreeText(const char* oldBuf, const char* newBuf,
                                   int centerX, int centerY,
                                   const GFXfont* gfxFont,
                                   uint16_t color, uint16_t bg) {
  if (strcmp(oldBuf, newBuf) == 0) return;

  tft.setTextDatum(MC_DATUM);
  tft.setFreeFont(gfxFont);

  int oldW = oldBuf[0] ? tft.textWidth(oldBuf) : 0;
  int newW = tft.textWidth(newBuf);
  int clearW = (oldW > newW ? oldW : newW) + 4;
  int clearH = tft.fontHeight() + 2;
  tft.fillRect(centerX - clearW / 2, centerY - clearH / 2, clearW, clearH, bg);

  tft.setTextColor(color, bg);
  tft.drawString(newBuf, centerX, centerY);
}

// ---------------------------------------------------------------------------
//  Draw a Twemoji weather bitmap centred at (cx, cy).
//  Erases the previous icon area, then blits the PROGMEM RGB565 array.
//  WI_TRANSP pixels are skipped (transparent).
// ---------------------------------------------------------------------------
static void drawWeatherIcon(uint8_t code, int cx, int cy, uint16_t bg) {
  const uint16_t* img;
  if      (code == 0)                        img = wi_clear;
  else if (code == 1)                        img = wi_mainly_clear;
  else if (code == 2)                        img = wi_partly_cloudy;
  else if (code == 3)                        img = wi_overcast;
  else if (code == 45 || code == 48)         img = wi_fog;
  else if (code >= 51 && code <= 57)         img = wi_drizzle;
  else if (code >= 61 && code <= 67)         img = wi_rain;
  else if (code >= 71 && code <= 77)         img = wi_snow;
  else if (code >= 80 && code <= 84)         img = wi_drizzle;   // rain showers
  else if (code == 85 || code == 86)         img = wi_snow;      // snow showers
  else if (code >= 95)                       img = wi_thunder;
  else                                       img = wi_overcast;  // fallback

  int x = cx - (int)(WI_W / 2);
  int y = cy - (int)(WI_H / 2);
  tft.fillRect(x, y, WI_W, WI_H, bg);
  // Draw pixel-by-pixel via pgm_read_word to avoid TFT_eSPI byte-swap quirks
  // with pushImage's transparent-color overload.
  // Icon only redraws on WMO code change so performance is not critical.
  tft.startWrite();
  for (int row = 0; row < (int)WI_H; row++) {
    for (int col = 0; col < (int)WI_W; col++) {
      uint16_t c = pgm_read_word(&img[row * WI_W + col]);
      if (c != WI_TRANSP) tft.drawPixel(x + col, y + row, c);
    }
  }
  tft.endWrite();
}

void drawWeatherClock() {
  // Trigger a weather fetch if due (may briefly block ~0.5-2s)
  fetchWeatherIfNeeded();

  struct tm now;
  if (!getLocalTime(&now, 0)) {
    time_t t = time(nullptr);
    if (t < 1600000000UL) return;
    localtime_r(&t, &now);
  }

  uint16_t bg       = dispSettings.bgColor;
  uint16_t timeClr  = dispSettings.clockTimeColor;
  uint16_t dateClr  = dispSettings.clockDateColor;
  uint16_t tempClr  = weatherSettings.tempColor;
  uint16_t infoClr  = weatherSettings.infoColor;

  // --- First draw: paint the divider line and city name background area ---
  if (wck_firstDraw) {
    // Clear the full clock/weather area
    tft.fillRect(0, 0, LY_W, LY_H, bg);
    // Draw a subtle divider between date and weather area
    tft.drawFastHLine(20, LY_WCK_DIV_Y, LY_W - 40, dispSettings.trackColor);
    wck_firstDraw = false;
    // Force redraw of city so it renders right away
    wck_prevCond[0] = '\1';  // force mismatch
  }

  // --- Colon blink ---
  bool colonOn = (millis() % 1000) < 500;
  if (colonOn != wck_prevColon) {
    int cx = wckDigitX(2);
    int cy = LY_WCK_TIME_Y - WCK_DIGIT_H / 2;
    tft.fillRect(cx, cy, WCK_COLON_W, WCK_DIGIT_H, bg);
    if (colonOn) {
      tft.setTextFont(7);
      tft.setTextSize(1);
      tft.setTextColor(timeClr, bg);
      tft.drawChar(':', cx, cy, 7);
    }
    wck_prevColon = colonOn;
  }

  // --- Only update time / date once per minute ---
  if (now.tm_min != wck_prevMinute) {
    wck_prevMinute = now.tm_min;

    // Build digit array
    char digits[5];
    if (netSettings.use24h) {
      digits[0] = '0' + (now.tm_hour / 10);
      digits[1] = '0' + (now.tm_hour % 10);
    } else {
      int h = now.tm_hour % 12;
      if (h == 0) h = 12;
      digits[0] = (h >= 10) ? '1' : ' ';
      digits[1] = '0' + (h % 10);
    }
    digits[2] = ':';
    digits[3] = '0' + (now.tm_min / 10);
    digits[4] = '0' + (now.tm_min % 10);

    // Draw changed digits
    tft.setTextFont(7);
    tft.setTextSize(1);
    tft.setTextColor(timeClr, bg);
    int dy = LY_WCK_TIME_Y - WCK_DIGIT_H / 2;
    for (int i = 0; i < 5; i++) {
      if (i == 2) continue;
      if (digits[i] == wck_prevDigits[i]) continue;
      int x = wckDigitX(i);
      tft.fillRect(x, dy, WCK_DIGIT_W + 2, WCK_DIGIT_H, bg);
      tft.drawChar(digits[i], x, dy, 7);
      wck_prevDigits[i] = digits[i];
    }
    if (wck_prevDigits[2] == 0) {
      wck_prevColon = !colonOn;
      wck_prevDigits[2] = ':';
    }

    // AM/PM (12h mode)
    if (!netSettings.use24h) {
      const char* ampm = (now.tm_hour < 12) ? "AM" : "PM";
      if (strcmp(ampm, wck_prevAmPm) != 0) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextFont(4);
        tft.setTextColor(dateClr, bg);
        int ampmW = tft.textWidth("PM");
        tft.fillRect(LY_W / 2 - ampmW / 2 - 2, LY_WCK_AMPM_Y - 12, ampmW + 4, 24, bg);
        tft.drawString(ampm, LY_W / 2, LY_WCK_AMPM_Y);
        strlcpy(wck_prevAmPm, ampm, sizeof(wck_prevAmPm));
      }
    } else if (wck_prevAmPm[0] != '\0') {
      tft.setTextDatum(MC_DATUM);
      tft.setTextFont(4);
      int ampmW = tft.textWidth("PM");
      tft.fillRect(LY_W / 2 - ampmW / 2 - 2, LY_WCK_AMPM_Y - 12, ampmW + 4, 24, bg);
      wck_prevAmPm[0] = '\0';
    }

    // Date
    const char* days[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                             "Jul","Aug","Sep","Oct","Nov","Dec"};
    char dateBuf[28];
    int day = now.tm_mday, mon = now.tm_mon + 1, year = now.tm_year + 1900;
    switch (netSettings.dateFormat) {
      case 1:  snprintf(dateBuf, sizeof(dateBuf), "%s  %02d-%02d-%04d", days[now.tm_wday], day, mon, year); break;
      case 2:  snprintf(dateBuf, sizeof(dateBuf), "%s  %02d/%02d/%04d", days[now.tm_wday], mon, day, year); break;
      case 3:  snprintf(dateBuf, sizeof(dateBuf), "%s  %04d-%02d-%02d", days[now.tm_wday], year, mon, day); break;
      case 4:  snprintf(dateBuf, sizeof(dateBuf), "%s  %d %s %04d", days[now.tm_wday], day, months[now.tm_mon], year); break;
      case 5:  snprintf(dateBuf, sizeof(dateBuf), "%s  %s %d, %04d", days[now.tm_wday], months[now.tm_mon], day, year); break;
      default: snprintf(dateBuf, sizeof(dateBuf), "%s  %02d.%02d.%04d", days[now.tm_wday], day, mon, year); break;
    }
    redrawCentredText(wck_prevDateBuf, dateBuf, LY_W / 2, LY_WCK_DATE_Y, 4, dateClr, bg);
    strlcpy(wck_prevDateBuf, dateBuf, sizeof(wck_prevDateBuf));
  }

  // --- Weather section (update whenever data changes) ---
  // City name (static label entered by the user; only redraws on change/first draw)
  {
    const char* city = weatherSettings.city[0] ? weatherSettings.city : "Weather";
    if (strcmp(wck_prevCity, city) != 0) {
      tft.setFreeFont(&FreeSans12pt7b);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(infoClr, bg);
      int cW = wck_prevCity[0] ? tft.textWidth(wck_prevCity) : 0;
      int nW = tft.textWidth(city);
      int clW = (cW > nW ? cW : nW) + 4;
      int fh = tft.fontHeight();
      tft.fillRect(WCK_TEXT_CX - clW / 2, LY_WCK_CITY_Y - fh / 2 - 1, clW, fh + 2, bg);
      tft.drawString(city, WCK_TEXT_CX, LY_WCK_CITY_Y);
      strlcpy(wck_prevCity, city, sizeof(wck_prevCity));
    }
  }

  if (!weatherData.valid) {
    // Show "Loading..." only once until data arrives
    static bool loadingShown = false;
    if (!loadingShown) {
      tft.setFreeFont(&FreeSans12pt7b);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(infoClr, bg);
      tft.drawString("Fetching weather...", WCK_TEXT_CX, LY_WCK_TEMP_Y);
      loadingShown = true;
    }
    return;
  }

  // --- Weather icon (only redraws when WMO code changes) ---
  if (weatherData.weatherCode != wck_prevCode) {
    drawWeatherIcon(weatherData.weatherCode, LY_WCK_ICON_X, LY_WCK_ICON_Y, bg);
    wck_prevCode = weatherData.weatherCode;
  }

  // Temperature line: "15.0°C" (font 4 + manual degree circle)
  // Font 4 only covers ASCII 32-127; degree symbol (0xB0) is drawn as a small circle.
  const char* unit  = weatherSettings.useMetric ? "C" : "F";
  char tempBuf[16];
  snprintf(tempBuf, sizeof(tempBuf), "%.1f%s", weatherData.temp, unit); // no degree in str
  if (strcmp(wck_prevTemp, tempBuf) != 0) {
    tft.setTextFont(4);
    tft.setTextDatum(TL_DATUM);
    char numBuf[12];
    snprintf(numBuf, sizeof(numBuf), "%.1f", weatherData.temp);
    int numW  = tft.textWidth(numBuf);
    int unitW = tft.textWidth(unit);
    int fh    = tft.fontHeight(4);
    const int degR = 3, degGap = 2;
    int totalW = numW + degGap + degR * 2 + degGap + unitW;
    int startX = WCK_TEXT_CX - totalW / 2;
    int topY   = LY_WCK_TEMP_Y - fh / 2;
    // clear
    int oldW = wck_prevTemp[0] ? (tft.textWidth(wck_prevTemp) + 12) : 0;
    int clrW = (oldW > totalW + 12 ? oldW : totalW + 12);
    tft.fillRect(WCK_TEXT_CX - clrW / 2, topY - 2, clrW, fh + 4, bg);
    // draw number
    tft.setTextColor(tempClr, bg);
    tft.drawString(numBuf, startX, topY);
    // draw degree circle (top of font)
    tft.drawCircle(startX + numW + degGap + degR, topY + degR + 1, degR, tempClr);
    // draw unit
    tft.drawString(unit, startX + numW + degGap + degR * 2 + degGap, topY);
    strlcpy(wck_prevTemp, tempBuf, sizeof(wck_prevTemp));
  }

  // Condition text (font 2)
  char condBuf[48];
  strlcpy(condBuf, wmoConditionString(weatherData.weatherCode), sizeof(condBuf));
  redrawCentredFreeText(wck_prevCond, condBuf, WCK_TEXT_CX, LY_WCK_COND_Y, &FreeSans12pt7b, infoClr, bg);
  strlcpy(wck_prevCond, condBuf, sizeof(wck_prevCond));

  // Extra line: humidity · wind · feels-like  (FreeSans9pt7b + manual degree circle on FL)
  char extraBuf[48];
  snprintf(extraBuf, sizeof(extraBuf), "H:%d%% W:%.0f FL:%.0f%s",
    (int)weatherData.humidity, weatherData.windSpeed, weatherData.feelsLike, unit);
  if (strcmp(wck_prevExtra, extraBuf) != 0) {
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextDatum(TL_DATUM);
    char mainBuf[40];
    snprintf(mainBuf, sizeof(mainBuf), "H:%d%% W:%.0f FL:%.0f",
      (int)weatherData.humidity, weatherData.windSpeed, weatherData.feelsLike);
    int mainW = tft.textWidth(mainBuf);
    int unitW = tft.textWidth(unit);
    const int degR = 3, degGap = 2;
    int totalW = mainW + degGap + degR * 2 + degGap + unitW;
    int fh = tft.fontHeight();
    int topY = LY_WCK_EXTRA_Y - fh / 2;
    int startX = WCK_TEXT_CX - totalW / 2;
    // Clear enough for both old and new text
    int oldW = wck_prevExtra[0] ? tft.textWidth(wck_prevExtra) + 16 : 0;
    int clrW = (oldW > totalW + 16) ? oldW : totalW + 16;
    tft.fillRect(WCK_TEXT_CX - clrW / 2, topY - 2, clrW, fh + 4, bg);
    // Draw main text, degree circle, unit
    tft.setTextColor(infoClr, bg);
    tft.drawString(mainBuf, startX, topY);
    tft.drawCircle(startX + mainW + degGap + degR, topY + degR + 2, degR, infoClr);
    tft.drawString(unit, startX + mainW + degGap + degR * 2 + degGap, topY);
    strlcpy(wck_prevExtra, extraBuf, sizeof(wck_prevExtra));
  }
}
