#include "buzzer.h"
#include "settings.h"
#include "config.h"
#include <time.h>

// ---------------------------------------------------------------------------
//  Tone patterns (frequency Hz, duration ms) — 0 freq = pause
// ---------------------------------------------------------------------------
struct ToneStep { uint16_t freq; uint16_t ms; };

// Print finished: cheerful ascending melody
static const ToneStep melodyFinished[] = {
  {1047, 120}, {0, 40},   // C6
  {1319, 120}, {0, 40},   // E6
  {1568, 120}, {0, 40},   // G6
  {2093, 250},             // C7 (longer final note)
};

// Error: three short warning beeps
static const ToneStep melodyError[] = {
  {880, 100}, {0, 80},
  {880, 100}, {0, 80},
  {880, 100},
};

// Connected: two quick ascending tones
static const ToneStep melodyConnected[] = {
  {1047, 80}, {0, 40},
  {1568, 120},
};

// Button click: single short tick
static const ToneStep melodyClick[] = {
  {4000, 8},
};

// ---------------------------------------------------------------------------
//  Non-blocking playback state
// ---------------------------------------------------------------------------
static const ToneStep* currentMelody = nullptr;
static uint8_t melodyLen = 0;
static uint8_t melodyIdx = 0;
static unsigned long stepStartMs = 0;
static bool playing = false;

void initBuzzer() {
  if (buzzerSettings.enabled && buzzerSettings.pin > 0) {
    pinMode(buzzerSettings.pin, OUTPUT);
    digitalWrite(buzzerSettings.pin, LOW);
  }
}

bool buzzerIsQuietHour() {
  uint8_t qs = buzzerSettings.quietStartHour;
  uint8_t qe = buzzerSettings.quietEndHour;
  if (qs == qe) return false;  // quiet hours disabled

  struct tm now;
  if (!getLocalTime(&now, 0)) return false;
  uint8_t h = now.tm_hour;

  if (qs < qe) {
    return h >= qs && h < qe;        // e.g. 22-07 doesn't wrap
  } else {
    return h >= qs || h < qe;        // e.g. 22:00 to 07:00 wraps midnight
  }
}

void buzzerPlay(BuzzerEvent event) {
  if (!buzzerSettings.enabled) return;
  if (buzzerIsQuietHour()) return;
  if (playing) return;  // don't interrupt

  switch (event) {
    case BUZZ_PRINT_FINISHED:
      currentMelody = melodyFinished;
      melodyLen = sizeof(melodyFinished) / sizeof(ToneStep);
      break;
    case BUZZ_ERROR:
      currentMelody = melodyError;
      melodyLen = sizeof(melodyError) / sizeof(ToneStep);
      break;
    case BUZZ_CONNECTED:
      currentMelody = melodyConnected;
      melodyLen = sizeof(melodyConnected) / sizeof(ToneStep);
      break;
    case BUZZ_CLICK:
      currentMelody = melodyClick;
      melodyLen = sizeof(melodyClick) / sizeof(ToneStep);
      break;
    default: return;
  }

  melodyIdx = 0;
  stepStartMs = millis();
  playing = true;

  // Start first tone
  if (currentMelody[0].freq > 0) {
    tone(buzzerSettings.pin, currentMelody[0].freq);
  }
}

void buzzerPlayClick() {
  if (!buzzerSettings.enabled || buzzerSettings.pin == 0) return;
  if (!buzzerSettings.buttonClick) return;
  // Click ignores quiet hours - it's user-initiated tactile feedback.
  // Played synchronously (8ms delay is imperceptible) so the tone always
  // completes cleanly even if the next loop iteration blocks on TLS.
  bool wasPlaying = playing;
  if (wasPlaying) noTone(buzzerSettings.pin);
  tone(buzzerSettings.pin, melodyClick[0].freq);
  delay(melodyClick[0].ms);
  noTone(buzzerSettings.pin);
  digitalWrite(buzzerSettings.pin, LOW);
  if (wasPlaying && currentMelody && melodyIdx < melodyLen) {
    stepStartMs = millis();
    if (currentMelody[melodyIdx].freq > 0)
      tone(buzzerSettings.pin, currentMelody[melodyIdx].freq);
  }
}

void buzzerTick() {
  if (!playing || !currentMelody) return;

  unsigned long elapsed = millis() - stepStartMs;
  if (elapsed < currentMelody[melodyIdx].ms) return;

  // Current step done, move to next
  noTone(buzzerSettings.pin);
  melodyIdx++;

  if (melodyIdx >= melodyLen) {
    // Melody complete
    playing = false;
    currentMelody = nullptr;
    digitalWrite(buzzerSettings.pin, LOW);
    return;
  }

  // Start next step
  stepStartMs = millis();
  if (currentMelody[melodyIdx].freq > 0) {
    tone(buzzerSettings.pin, currentMelody[melodyIdx].freq);
  }
}
