#include "display_anim.h"
#include "config.h"
#include "settings.h"
#include "icons.h"

// Use user-configured bg color instead of hardcoded CLR_BG
#undef  CLR_BG
#define CLR_BG  (dispSettings.bgColor)

// ---------------------------------------------------------------------------
//  Rotating arc spinner
// ---------------------------------------------------------------------------
static uint16_t spinnerAngle = 0;

void drawSpinner(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t radius,
                 uint16_t color) {
  // Erase previous arc segment (handle wrap-around)
  uint16_t prevStart = (spinnerAngle + 360 - 12) % 360;
  uint16_t prevEnd = (prevStart + 60) % 360;
  if (prevEnd > prevStart) {
    tft.drawSmoothArc(cx, cy, radius, radius - 4,
                      prevStart, prevEnd, CLR_BG, CLR_BG, false);
  } else {
    tft.drawSmoothArc(cx, cy, radius, radius - 4,
                      prevStart, 360, CLR_BG, CLR_BG, false);
    tft.drawSmoothArc(cx, cy, radius, radius - 4,
                      0, prevEnd, CLR_BG, CLR_BG, false);
  }

  // Advance angle
  spinnerAngle = (spinnerAngle + 12) % 360;
  uint16_t arcStart = spinnerAngle;
  uint16_t arcEnd = (spinnerAngle + 60) % 360;

  // Draw arc segment (handle wrap-around)
  if (arcEnd > arcStart) {
    tft.drawSmoothArc(cx, cy, radius, radius - 4,
                      arcStart, arcEnd, color, CLR_BG, false);
  } else {
    tft.drawSmoothArc(cx, cy, radius, radius - 4,
                      arcStart, 360, color, CLR_BG, false);
    tft.drawSmoothArc(cx, cy, radius, radius - 4,
                      0, arcEnd, color, CLR_BG, false);
  }
}

// ---------------------------------------------------------------------------
//  Animated dots "..."
// ---------------------------------------------------------------------------
void drawAnimDots(TFT_eSPI& tft, int16_t x, int16_t y, uint16_t color) {
  unsigned long ms = millis();
  int phase = (ms / 400) % 4;

  tft.setTextFont(2);
  tft.setTextDatum(TL_DATUM);

  for (int i = 0; i < 3; i++) {
    uint16_t dotColor = (i < phase) ? color : CLR_TEXT_DARK;
    tft.setTextColor(dotColor, CLR_BG);
    tft.drawString(".", x + i * 8, y);
  }
}

// ---------------------------------------------------------------------------
//  Pulse factor for glow effects
// ---------------------------------------------------------------------------
float getPulseFactor() {
  unsigned long ms = millis();
  float t = (ms % 2000) / 2000.0f;
  float sine = sinf(t * 2.0f * PI);
  return 0.75f + 0.25f * sine;
}

// ---------------------------------------------------------------------------
//  Completion animation — expanding ring + large checkmark
// ---------------------------------------------------------------------------
static unsigned long completionStart = 0;
static bool completionDone = false;
static int16_t prevRing = 0;

void drawCompletionAnim(TFT_eSPI& tft, int16_t cx, int16_t cy, bool reset) {
  if (reset) {
    completionStart = millis();
    completionDone = false;
    prevRing = 0;
    return;
  }

  if (completionDone) return;

  unsigned long elapsed = millis() - completionStart;
  const int16_t finalR = 45;

  // Phase 1 (0-400ms): expanding green ring
  if (elapsed < 400) {
    int16_t r = 10 + (elapsed * (finalR - 10)) / 400;
    // Erase previous ring
    if (prevRing > 0 && prevRing != r) {
      tft.drawSmoothArc(cx, cy, prevRing, prevRing - 3, 0, 360, CLR_BG, CLR_BG, false);
    }
    tft.drawSmoothArc(cx, cy, r, r - 3, 0, 360, CLR_GREEN, CLR_BG, false);
    prevRing = r;
  }
  // Phase 2 (400-600ms): settle to final ring
  else if (elapsed < 600) {
    tft.drawSmoothArc(cx, cy, finalR, finalR - 4, 0, 360, CLR_GREEN, CLR_BG, false);
  }
  // Phase 3 (600ms+): static ring + large checkmark, done
  else {
    tft.drawSmoothArc(cx, cy, finalR, finalR - 4, 0, 360, CLR_GREEN, CLR_BG, false);
    // Clear center for checkmark
    tft.fillCircle(cx, cy, finalR - 5, CLR_BG);
    // Draw 32x32 checkmark centered
    drawIcon32(tft, cx - 16, cy - 16, icon_check_32, CLR_GREEN);
    completionDone = true;
  }
}
