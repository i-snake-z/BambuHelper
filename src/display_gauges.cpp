#include "display_gauges.h"
#include "config.h"
#include "settings.h"

// ---------------------------------------------------------------------------
//  H2-style LED progress bar
// ---------------------------------------------------------------------------
void drawLedProgressBar(TFT_eSPI& tft, int16_t y, uint8_t progress) {
  uint16_t bg = dispSettings.bgColor;
  uint16_t track = dispSettings.trackColor;

  const int16_t barW = 236;
  const int16_t barH = 5;
  const int16_t barX = (SCREEN_W - barW) / 2;

  tft.fillRect(barX, y, barW, barH, bg);

  if (progress == 0) return;

  int16_t fillW = (progress * barW) / 100;
  if (fillW < 1) fillW = 1;

  uint16_t barColor = dispSettings.progress.arc;

  tft.fillRoundRect(barX, y, fillW, barH, 2, barColor);

  uint16_t glowColor = tft.alphaBlend(160, CLR_TEXT, barColor);
  tft.drawFastHLine(barX + 1, y + barH / 2, fillW - 2, glowColor);

  if (fillW > 4 && progress < 100) {
    tft.fillRect(barX + fillW - 3, y, 3, barH, glowColor);
  }

  if (fillW < barW) {
    tft.fillRoundRect(barX + fillW, y, barW - fillW, barH, 2, track);
  }
}

// ---------------------------------------------------------------------------
//  Shimmer animation for progress bar
// ---------------------------------------------------------------------------
static int16_t shimmerPos = -1;       // current x offset within filled area
static unsigned long shimmerLastMs = 0;
static bool shimmerPaused = false;
static unsigned long shimmerPauseStart = 0;

static const int16_t SHIMMER_W = 12;       // width of highlight
static const uint16_t SHIMMER_INTERVAL = 25;  // ms between steps (~40fps)
static const uint16_t SHIMMER_PAUSE = 1200;   // ms pause between sweeps
static const int16_t SHIMMER_STEP = 3;       // pixels per step

void tickProgressShimmer(TFT_eSPI& tft, int16_t y, uint8_t progress, bool printing) {
  if (!dispSettings.animatedBar || !printing || progress == 0) return;

  unsigned long now = millis();

  // Handle pause between sweeps
  if (shimmerPaused) {
    if (now - shimmerPauseStart < SHIMMER_PAUSE) return;
    shimmerPaused = false;
    shimmerPos = 0;
  }

  if (now - shimmerLastMs < SHIMMER_INTERVAL) return;
  shimmerLastMs = now;

  const int16_t barW = 236;
  const int16_t barH = 5;
  const int16_t barX = (SCREEN_W - barW) / 2;
  int16_t fillW = (progress * barW) / 100;
  if (fillW < SHIMMER_W + 4) return;  // too small for shimmer

  uint16_t barColor = dispSettings.progress.arc;

  // Erase previous shimmer position (redraw base bar segment)
  if (shimmerPos > 0) {
    int16_t eraseX = barX + shimmerPos - SHIMMER_STEP;
    int16_t eraseW = SHIMMER_STEP;
    if (eraseX < barX) { eraseW -= (barX - eraseX); eraseX = barX; }
    if (eraseW > 0) {
      tft.fillRect(eraseX, y, eraseW, barH, barColor);
    }
  }

  // Draw shimmer highlight
  int16_t sx = barX + shimmerPos;
  int16_t sw = SHIMMER_W;
  if (sx + sw > barX + fillW) sw = barX + fillW - sx;
  if (sw > 0) {
    // Gradient-like shimmer: brighter in center
    uint16_t bright = tft.alphaBlend(180, CLR_TEXT, barColor);
    uint16_t mid    = tft.alphaBlend(100, CLR_TEXT, barColor);
    // Edge pixels
    if (sw >= 3) {
      tft.fillRect(sx, y, 2, barH, mid);
      tft.fillRect(sx + 2, y, sw - 4 > 0 ? sw - 4 : 1, barH, bright);
      if (sw > 4) tft.fillRect(sx + sw - 2, y, 2, barH, mid);
    } else {
      tft.fillRect(sx, y, sw, barH, bright);
    }
  }

  shimmerPos += SHIMMER_STEP;

  // Reached end of filled area — restore last segment and pause
  if (shimmerPos >= fillW) {
    // Restore the tail
    int16_t tailX = barX + fillW - SHIMMER_W - SHIMMER_STEP;
    if (tailX < barX) tailX = barX;
    tft.fillRect(tailX, y, barX + fillW - tailX, barH, barColor);
    // Re-draw center glow line
    uint16_t glowColor = tft.alphaBlend(160, CLR_TEXT, barColor);
    tft.drawFastHLine(barX + 1, y + barH / 2, fillW - 2, glowColor);

    shimmerPos = 0;
    shimmerPaused = true;
    shimmerPauseStart = now;
  }
}

// ---------------------------------------------------------------------------
//  Helper: draw arc track + fill, handling decrease properly
// ---------------------------------------------------------------------------
static void drawArcFill(TFT_eSPI& tft, int16_t cx, int16_t cy,
                        int16_t radius, int16_t thickness,
                        uint16_t fillEnd, uint16_t fillColor, bool forceRedraw) {
  const uint16_t startAngle = 60;
  const uint16_t endAngle = 300;
  uint16_t bg = dispSettings.bgColor;
  uint16_t track = dispSettings.trackColor;

  if (forceRedraw) {
    tft.fillCircle(cx, cy, radius + 2, bg);
    tft.drawSmoothArc(cx, cy, radius, radius - thickness,
                      startAngle, endAngle, track, bg, false);
  }

  // Draw filled portion
  if (fillEnd > startAngle) {
    tft.drawSmoothArc(cx, cy, radius, radius - thickness,
                      startAngle, fillEnd, fillColor, bg, false);
  }

  // Always redraw track for unfilled portion (handles value decrease)
  if (fillEnd < endAngle) {
    tft.drawSmoothArc(cx, cy, radius, radius - thickness,
                      fillEnd, endAngle, track, bg, false);
  }
}

// ---------------------------------------------------------------------------
//  Helper: clear gauge center and prepare for text
// ---------------------------------------------------------------------------
static void clearGaugeCenter(TFT_eSPI& tft, int16_t cx, int16_t cy,
                             int16_t radius, int16_t thickness) {
  int16_t textR = radius - thickness - 1;
  tft.fillCircle(cx, cy, textR, dispSettings.bgColor);
}

// ---------------------------------------------------------------------------
//  Main progress arc
// ---------------------------------------------------------------------------
void drawProgressArc(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t radius,
                     int16_t thickness, uint8_t progress, uint8_t prevProgress,
                     uint16_t remainingMin, bool forceRedraw) {
  const uint16_t startAngle = 60;
  const GaugeColors& gc = dispSettings.progress;
  uint16_t bg = dispSettings.bgColor;

  uint16_t fillEnd = startAngle + (progress * 240) / 100;
  if (fillEnd > 300) fillEnd = 300;

  drawArcFill(tft, cx, cy, radius, thickness, fillEnd, gc.arc, forceRedraw);
  clearGaugeCenter(tft, cx, cy, radius, thickness);

  bool compact = (radius < 50);

  // Percentage value (no bg color — clearGaugeCenter already filled the circle)
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(gc.value);
  tft.setTextFont(compact ? 4 : 6);
  char pctBuf[8];
  if (compact) {
    snprintf(pctBuf, sizeof(pctBuf), "%d", progress);
  } else {
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", progress);
  }
  tft.drawString(pctBuf, cx, cy - (compact ? 4 : 8));

  // Time remaining
  tft.setTextFont(compact ? 1 : 2);
  tft.setTextColor(CLR_TEXT_DIM);
  char timeBuf[16];
  if (remainingMin >= 60) {
    snprintf(timeBuf, sizeof(timeBuf), "%dh%dm", remainingMin / 60, remainingMin % 60);
  } else {
    snprintf(timeBuf, sizeof(timeBuf), "%dm", remainingMin);
  }
  tft.drawString(timeBuf, cx, cy + (compact ? 10 : 18));

  // Label below gauge
  if (compact) {
    tft.setTextFont(1);
    tft.setTextColor(gc.label, bg);
    tft.drawString("Progress", cx, cy + radius + 6);
  }
}

// ---------------------------------------------------------------------------
//  Temperature arc gauge
// ---------------------------------------------------------------------------
void drawTempGauge(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t radius,
                   float current, float target, float maxTemp,
                   uint16_t accentColor, const char* label,
                   const uint8_t* icon, bool forceRedraw,
                   const GaugeColors* colors) {
  const uint16_t startAngle = 60;
  const int16_t thickness = 6;
  uint16_t bg = dispSettings.bgColor;

  // Use custom colors if provided, otherwise fall back to accentColor
  uint16_t arcColor = colors ? colors->arc : accentColor;
  uint16_t lblColor = colors ? colors->label : accentColor;
  uint16_t valColor = colors ? colors->value : CLR_TEXT;

  float ratio = (maxTemp > 0) ? (current / maxTemp) : 0;
  if (ratio > 1.0f) ratio = 1.0f;
  if (ratio < 0.0f) ratio = 0.0f;

  uint16_t fillEnd = startAngle + (uint16_t)(ratio * 240);
  if (fillEnd <= startAngle && ratio > 0.01f) fillEnd = startAngle + 1;
  if (fillEnd > 300) fillEnd = 300;

  // Use custom arc color for all fill levels
  uint16_t tempColor = arcColor;

  uint16_t drawFill = (ratio > 0.01f) ? fillEnd : startAngle;
  drawArcFill(tft, cx, cy, radius, thickness, drawFill, tempColor, forceRedraw);
  clearGaugeCenter(tft, cx, cy, radius, thickness);

  // Current temp (no bg color — clearGaugeCenter already filled the circle)
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(valColor);
  char tempBuf[12];
  snprintf(tempBuf, sizeof(tempBuf), "%.0f", current);
  tft.drawString(tempBuf, cx, cy - 4);

  // Target temp
  tft.setTextFont(1);
  tft.setTextColor(CLR_TEXT_DIM);
  snprintf(tempBuf, sizeof(tempBuf), "/%.0f", target);
  tft.drawString(tempBuf, cx, cy + 10);

  // Label below gauge
  tft.setTextColor(lblColor, bg);
  tft.drawString(label, cx, cy + radius + 6);
}

// ---------------------------------------------------------------------------
//  Fan speed gauge (0-100%)
// ---------------------------------------------------------------------------
void drawFanGauge(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t radius,
                  uint8_t percent, uint16_t accentColor, const char* label,
                  bool forceRedraw, const GaugeColors* colors) {
  const uint16_t startAngle = 60;
  const int16_t thickness = 6;
  uint16_t bg = dispSettings.bgColor;

  uint16_t arcColor = colors ? colors->arc : accentColor;
  uint16_t lblColor = colors ? colors->label : accentColor;
  uint16_t valColor = colors ? colors->value : CLR_TEXT;

  uint16_t fillEnd = startAngle + (percent * 240) / 100;
  if (fillEnd > 300) fillEnd = 300;

  uint16_t fanColor;
  if (percent == 0) {
    fanColor = CLR_TEXT_DIM;
  } else {
    fanColor = arcColor;
  }

  uint16_t drawFill = (percent > 0) ? fillEnd : startAngle;
  drawArcFill(tft, cx, cy, radius, thickness, drawFill, fanColor, forceRedraw);
  clearGaugeCenter(tft, cx, cy, radius, thickness);

  // Percentage value (no bg color — clearGaugeCenter already filled the circle)
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(valColor);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", percent);
  tft.drawString(buf, cx, cy);

  // Label below gauge
  tft.setTextFont(1);
  tft.setTextColor(lblColor, bg);
  tft.drawString(label, cx, cy + radius + 6);
}
