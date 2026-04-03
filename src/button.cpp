#include "button.h"
#include "settings.h"

#if defined(USE_XPT2046)
  #include <SPI.h>
  #include <XPT2046_Touchscreen.h>
  static SPIClass touchSPI(HSPI);
  static XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);
  static bool touchReady = false;
#elif defined(TOUCH_CS)
  #include "display_ui.h"  // extern tft for getTouch()
#endif

// --- Shared debounced state (updated once per loop via pollButton) ---
static bool lastRaw      = false;
static bool stableState  = false;
static unsigned long lastChangeMs  = 0;
static bool shortPressFired = false;  // edge flag, consumed by wasButtonPressed()

static const unsigned long DEBOUNCE_MS   = 50;

static bool readRaw() {
  if (buttonType == BTN_TOUCHSCREEN) {
#if defined(USE_XPT2046)
    if (!touchReady) return false;
    return ts.touched();
#elif defined(TOUCH_CS)
    uint16_t tx, ty;
    return tft.getTouch(&tx, &ty);
#else
    return false;
#endif
  } else if (buttonType == BTN_PUSH) {
    if (buttonPin == 0) return false;
    return (digitalRead(buttonPin) == LOW);   // active LOW with pull-up
  } else {  // BTN_TOUCH
    if (buttonPin == 0) return false;
    return (digitalRead(buttonPin) == HIGH);  // TTP223: active HIGH
  }
}

void initButton() {
  if (buttonType == BTN_DISABLED) return;
#if defined(USE_XPT2046)
  if (buttonType == BTN_TOUCHSCREEN) {
    touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    ts.begin(touchSPI);
    touchReady = true;
    Serial.println("XPT2046 touch initialized (separate SPI)");
    return;
  }
#endif
  if (buttonType == BTN_TOUCHSCREEN) return;
  if (buttonPin == 0) return;
  if (buttonType == BTN_PUSH) {
    pinMode(buttonPin, INPUT_PULLUP);
  } else {  // BTN_TOUCH (TTP223)
    pinMode(buttonPin, INPUT);
  }
  lastRaw = false;
  stableState = false;
  lastChangeMs = 0;
}

// Must be called once per main loop tick to update shared state.
void pollButton() {
  if (buttonType == BTN_DISABLED) return;

  bool raw = readRaw();

  // Debounce: track when raw state changes
  if (raw != lastRaw) {
    lastChangeMs = millis();
    lastRaw = raw;
  }

  // Only act on stable (debounced) state
  if ((millis() - lastChangeMs) < DEBOUNCE_MS) return;

  bool prevStable = stableState;
  stableState = raw;

  // Rising edge → short press
  if (raw && !prevStable) {
    shortPressFired = true;
  }
}

// Returns true once per rising edge (short tap).
bool wasButtonPressed() {
  if (!shortPressFired) return false;
  shortPressFired = false;
  return true;
}


