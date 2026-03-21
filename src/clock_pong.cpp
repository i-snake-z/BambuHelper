/*
 * Pong/Breakout animated clock for BambuHelper
 * Ported from TFTClock project, adapted for TFT_eSPI.
 *
 * Ball bounces off paddle and breaks colored brick rows.
 * On minute change, digits break apart with fragment effects.
 */

#include "clock_pong.h"
#include "config.h"
#include "settings.h"
#include "display_ui.h"
#include <TFT_eSPI.h>
#include <time.h>

extern TFT_eSPI tft;

// ========== Constants ==========
#define ARK_BRICK_ROWS    5
#define ARK_BRICK_COLS    10
#define ARK_BRICK_W       22
#define ARK_BRICK_H       8
#define ARK_BRICK_GAP     2
#define ARK_BRICK_START_X 3
#define ARK_BRICK_START_Y 30
#define ARK_BALL_SIZE     4
#define ARK_PADDLE_Y      220
#define ARK_PADDLE_H      4
#define ARK_PADDLE_W      30
#define ARK_TIME_Y        120
#define ARK_DATE_Y        6
#define ARK_BALL_SPEED    3.0f
#define ARK_PADDLE_SPEED  4
#define ARK_UPDATE_MS     20    // ~50fps
#define ARK_MAX_FRAGS     20
#define ARK_TIME_OVERRIDE_MS 60000

// Brick colors per row (classic Arcanoid rainbow)
static const uint16_t brickColors[ARK_BRICK_ROWS] = {
  0xF800,  // Red
  0xFD20,  // Orange
  0xFFE0,  // Yellow
  0x07E0,  // Green
  0x001F,  // Blue
};

// Digit X positions (textSize 4 = 24px wide, centered for 240px)
static const int DIGIT_X[5] = {40, 74, 108, 142, 176};

// ========== Fragment struct ==========
struct PongFragment {
  float x, y, vx, vy;
  bool active;
};

// ========== State ==========
static bool arkBricks[ARK_BRICK_ROWS][ARK_BRICK_COLS];
static int arkBrickCount = 0;

static float ballX, ballY, ballVX, ballVY;
static bool ballActive = false;
static int prevBallX = -1, prevBallY = -1;

static int paddleX = 120, prevPaddleX = 120;

static bool initialized = false;
static unsigned long lastUpdateMs = 0;
static int lastMinute = -1;
static bool animTriggered = false;

// Digit transition
static int dispHour = 0, dispMin = 0;
static bool timeOverridden = false;
static unsigned long timeOverrideStart = 0;

static int targetDigits[4], targetValues[4];
static int numTargets = 0, currentTarget = 0;
static bool breaking = false;

static PongFragment frags[ARK_MAX_FRAGS];
static int fragTimer = 0;

// Digit bounce
static float digitOffsetY[5] = {0};
static float digitVelocity[5] = {0};
static int prevDigitY[5] = {0};

// ========== Digit bounce (inlined) ==========
static void triggerBounce(int i) {
  if (i >= 0 && i < 5) digitVelocity[i] = -6.0f;
}

static void updateBounce() {
  static unsigned long lastPhys = 0;
  unsigned long now = millis();
  float dt = (now - lastPhys) / 1000.0f;
  if (dt > 0.1f || lastPhys == 0) dt = 0.025f;
  lastPhys = now;
  float scale = dt / 0.05f;

  for (int i = 0; i < 5; i++) {
    if (digitOffsetY[i] != 0 || digitVelocity[i] != 0) {
      digitVelocity[i] += 3.0f * scale;
      digitOffsetY[i] += digitVelocity[i] * scale;
      if (digitOffsetY[i] >= 0) { digitOffsetY[i] = 0; digitVelocity[i] = 0; }
    }
  }
}

// ========== Colon blink ==========
static bool showColon() {
  return (millis() % 1000) < 500;
}

// ========== Bricks ==========
static void initBricks() {
  arkBrickCount = 0;
  for (int r = 0; r < ARK_BRICK_ROWS; r++)
    for (int c = 0; c < ARK_BRICK_COLS; c++) {
      arkBricks[r][c] = true;
      arkBrickCount++;
    }
}

static void drawBrick(int r, int c) {
  int x = ARK_BRICK_START_X + c * (ARK_BRICK_W + ARK_BRICK_GAP);
  int y = ARK_BRICK_START_Y + r * (ARK_BRICK_H + ARK_BRICK_GAP);
  tft.fillRect(x, y, ARK_BRICK_W, ARK_BRICK_H, brickColors[r]);
  tft.drawFastHLine(x, y, ARK_BRICK_W, TFT_WHITE);
  tft.drawFastHLine(x, y + ARK_BRICK_H - 1, ARK_BRICK_W, TFT_BLACK);
}

static void drawAllBricks() {
  for (int r = 0; r < ARK_BRICK_ROWS; r++)
    for (int c = 0; c < ARK_BRICK_COLS; c++)
      if (arkBricks[r][c]) drawBrick(r, c);
}

static void clearBrick(int r, int c) {
  int x = ARK_BRICK_START_X + c * (ARK_BRICK_W + ARK_BRICK_GAP);
  int y = ARK_BRICK_START_Y + r * (ARK_BRICK_H + ARK_BRICK_GAP);
  tft.fillRect(x, y, ARK_BRICK_W, ARK_BRICK_H, TFT_BLACK);
}

// ========== Ball ==========
static void spawnBall() {
  ballX = paddleX;
  ballY = ARK_PADDLE_Y - ARK_BALL_SIZE - 2;
  float angle = random(30, 150) * PI / 180.0f;
  ballVX = ARK_BALL_SPEED * cos(angle);
  ballVY = -ARK_BALL_SPEED * sin(angle);
  if (fabsf(ballVY) < 1.0f) ballVY = -2.0f;
  ballActive = true;
}

static void drawBall() {
  if (prevBallX >= 0)
    tft.fillRect(prevBallX, prevBallY, ARK_BALL_SIZE, ARK_BALL_SIZE, TFT_BLACK);
  tft.fillRect((int)ballX, (int)ballY, ARK_BALL_SIZE, ARK_BALL_SIZE, TFT_WHITE);
  prevBallX = (int)ballX;
  prevBallY = (int)ballY;
}

// ========== Paddle ==========
static void drawPaddle() {
  if (prevPaddleX != paddleX)
    tft.fillRect(prevPaddleX - ARK_PADDLE_W / 2, ARK_PADDLE_Y, ARK_PADDLE_W, ARK_PADDLE_H, TFT_BLACK);
  tft.fillRect(paddleX - ARK_PADDLE_W / 2, ARK_PADDLE_Y, ARK_PADDLE_W, ARK_PADDLE_H, TFT_CYAN);
  tft.drawFastHLine(paddleX - ARK_PADDLE_W / 2, ARK_PADDLE_Y, ARK_PADDLE_W, TFT_WHITE);
  prevPaddleX = paddleX;
}

static void updatePaddle() {
  int target = (ballActive && ballVY > 0) ? (int)ballX : 120;
  if (paddleX < target - 2) paddleX += ARK_PADDLE_SPEED;
  else if (paddleX > target + 2) paddleX -= ARK_PADDLE_SPEED;
  if (paddleX < ARK_PADDLE_W / 2) paddleX = ARK_PADDLE_W / 2;
  if (paddleX > SCREEN_W - ARK_PADDLE_W / 2) paddleX = SCREEN_W - ARK_PADDLE_W / 2;
}

// ========== Brick collision ==========
static bool checkBrickCollision() {
  int bx = (int)ballX, by = (int)ballY;
  for (int r = 0; r < ARK_BRICK_ROWS; r++) {
    for (int c = 0; c < ARK_BRICK_COLS; c++) {
      if (!arkBricks[r][c]) continue;
      int rx = ARK_BRICK_START_X + c * (ARK_BRICK_W + ARK_BRICK_GAP);
      int ry = ARK_BRICK_START_Y + r * (ARK_BRICK_H + ARK_BRICK_GAP);
      if (bx + ARK_BALL_SIZE > rx && bx < rx + ARK_BRICK_W &&
          by + ARK_BALL_SIZE > ry && by < ry + ARK_BRICK_H) {
        arkBricks[r][c] = false;
        arkBrickCount--;
        clearBrick(r, c);
        float oL = (bx + ARK_BALL_SIZE) - rx, oR = (rx + ARK_BRICK_W) - bx;
        float oT = (by + ARK_BALL_SIZE) - ry, oB = (ry + ARK_BRICK_H) - by;
        if (min(oL, oR) < min(oT, oB)) ballVX = -ballVX; else ballVY = -ballVY;
        return true;
      }
    }
  }
  return false;
}

// ========== Ball physics ==========
static void updateBallPhysics() {
  if (!ballActive) return;
  ballX += ballVX;
  ballY += ballVY;

  if (ballX <= 0) { ballX = 0; ballVX = fabsf(ballVX); }
  if (ballX >= SCREEN_W - ARK_BALL_SIZE) { ballX = SCREEN_W - ARK_BALL_SIZE; ballVX = -fabsf(ballVX); }
  if (ballY <= 0) { ballY = 0; ballVY = fabsf(ballVY); }

  // Paddle collision
  if (ballVY > 0 && ballY + ARK_BALL_SIZE >= ARK_PADDLE_Y && ballY < ARK_PADDLE_Y + ARK_PADDLE_H) {
    int pL = paddleX - ARK_PADDLE_W / 2, pR = paddleX + ARK_PADDLE_W / 2;
    if (ballX + ARK_BALL_SIZE >= pL && ballX <= pR) {
      ballY = ARK_PADDLE_Y - ARK_BALL_SIZE;
      float hit = (ballX + ARK_BALL_SIZE / 2.0f - pL) / (float)ARK_PADDLE_W;
      float angle = (150.0f - hit * 120.0f) * PI / 180.0f;
      ballVX = ARK_BALL_SPEED * cos(angle);
      ballVY = -ARK_BALL_SPEED * sin(angle);
      if (fabsf(ballVY) < 1.0f) ballVY = -1.5f;
    }
  }

  if (ballY > SCREEN_H) spawnBall();
  checkBrickCollision();
  if (arkBrickCount <= 0) { initBricks(); drawAllBricks(); }
}

// ========== Fragments ==========
static void spawnDigitFragments(int dx, int dy) {
  for (int i = 0; i < ARK_MAX_FRAGS; i++) {
    frags[i].active = true;
    frags[i].x = dx + random(0, 24);
    frags[i].y = dy + random(0, 32);
    frags[i].vx = random(-30, 30) / 10.0f;
    frags[i].vy = random(-40, -10) / 10.0f;
  }
  fragTimer = 30;
}

static void updateFragments() {
  if (fragTimer <= 0) return;
  fragTimer--;
  for (int i = 0; i < ARK_MAX_FRAGS; i++) {
    if (!frags[i].active) continue;
    tft.fillRect((int)frags[i].x, (int)frags[i].y, 3, 3, TFT_BLACK);
    frags[i].x += frags[i].vx;
    frags[i].y += frags[i].vy;
    frags[i].vy += 0.3f;
    if (frags[i].y > SCREEN_H || frags[i].x < -10 || frags[i].x > SCREEN_W + 10) {
      frags[i].active = false;
      continue;
    }
    tft.fillRect((int)frags[i].x, (int)frags[i].y, 3, 3, brickColors[random(0, ARK_BRICK_ROWS)]);
  }
  if (fragTimer == 0) {
    for (int i = 0; i < ARK_MAX_FRAGS; i++) {
      if (frags[i].active) {
        tft.fillRect((int)frags[i].x, (int)frags[i].y, 3, 3, TFT_BLACK);
        frags[i].active = false;
      }
    }
  }
}

// ========== Calculate which digits change ==========
static void calcTargets(int hour, int mn) {
  numTargets = 0;
  int newMin = mn + 1, newHour = hour;
  if (newMin >= 60) { newMin = 0; newHour = (newHour + 1) % 24; }
  int oldD[4] = {hour / 10, hour % 10, mn / 10, mn % 10};
  int newD[4] = {newHour / 10, newHour % 10, newMin / 10, newMin % 10};
  int map[4] = {0, 1, 3, 4};
  for (int i = 3; i >= 0; i--) {
    if (oldD[i] != newD[i]) {
      targetDigits[numTargets] = map[i];
      targetValues[numTargets] = newD[i];
      numTargets++;
    }
  }
}

// ========== Update a digit value after break ==========
static void applyDigitValue(int di, int dv) {
  int ht = dispHour / 10, ho = dispHour % 10;
  int mt = dispMin / 10, mo = dispMin % 10;
  if (di == 0) { ht = dv; dispHour = ht * 10 + ho; }
  else if (di == 1) { ho = dv; dispHour = ht * 10 + ho; }
  else if (di == 3) { mt = dv; dispMin = mt * 10 + mo; }
  else if (di == 4) { mo = dv; dispMin = mt * 10 + mo; }
}

// ========== Draw time digits ==========
static void drawTime() {
  tft.setTextSize(4);
  char digits[5];
  if (netSettings.use24h) {
    digits[0] = '0' + (dispHour / 10);
    digits[1] = '0' + (dispHour % 10);
  } else {
    int h = dispHour % 12;
    if (h == 0) h = 12;
    digits[0] = (h >= 10) ? '1' : ' ';
    digits[1] = '0' + (h % 10);
  }
  digits[2] = showColon() ? ':' : ' ';
  digits[3] = '0' + (dispMin / 10);
  digits[4] = '0' + (dispMin % 10);

  for (int i = 0; i < 5; i++) {
    int y = ARK_TIME_Y + (int)digitOffsetY[i];
    if (prevDigitY[i] != y && prevDigitY[i] != 0)
      tft.fillRect(DIGIT_X[i], prevDigitY[i], 24, 32, TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(DIGIT_X[i], y);
    tft.print(digits[i]);
    prevDigitY[i] = y;
  }

  // AM/PM for 12h mode
  if (!netSettings.use24h) {
    tft.setTextSize(1);
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.setCursor(DIGIT_X[4] + 26, ARK_TIME_Y + 20);
    tft.print(dispHour < 12 ? "AM" : "PM");
  }
}

// ========== Reset ==========
void resetPongClock() {
  initialized = false;
}

// ========== Tick (call from loop, runs at own cadence) ==========
void tickPongClock() {
  if (!dpSettings.showClockAfterFinish && !dpSettings.keepDisplayOn) return;
  if (!dispSettings.pongClock) return;

  struct tm now;
  if (!getLocalTime(&now, 0)) return;

  // First-time init
  if (!initialized) {
    tft.fillScreen(TFT_BLACK);
    initBricks();
    drawAllBricks();
    spawnBall();
    dispHour = now.tm_hour;
    dispMin = now.tm_min;
    initialized = true;
    lastMinute = now.tm_min;
    for (int i = 0; i < 5; i++) { prevDigitY[i] = 0; digitOffsetY[i] = 0; digitVelocity[i] = 0; }
    for (int i = 0; i < ARK_MAX_FRAGS; i++) frags[i].active = false;
    prevBallX = -1;
    prevPaddleX = paddleX;
  }

  // Time management
  if (!timeOverridden) { dispHour = now.tm_hour; dispMin = now.tm_min; }
  if (timeOverridden) {
    bool matches = (now.tm_hour == dispHour && now.tm_min == dispMin);
    bool timeout = (millis() - timeOverrideStart > ARK_TIME_OVERRIDE_MS);
    if (matches || timeout) {
      timeOverridden = false;
      if (timeout && !matches) { dispHour = now.tm_hour; dispMin = now.tm_min; }
    }
  }

  // Throttle to ~50fps
  unsigned long ms = millis();
  if (ms - lastUpdateMs < ARK_UPDATE_MS) return;
  lastUpdateMs = ms;

  int sec = now.tm_sec;
  int curMin = now.tm_min;

  // Detect minute change
  if (curMin != lastMinute) { lastMinute = curMin; animTriggered = false; }

  // Trigger digit transition at second 56
  if (sec >= 56 && !animTriggered && !breaking) {
    animTriggered = true;
    calcTargets(dispHour, dispMin);
    if (numTargets > 0) {
      currentTarget = 0;
      breaking = true;
      int di = targetDigits[0];
      spawnDigitFragments(DIGIT_X[di], ARK_TIME_Y);
      tft.fillRect(DIGIT_X[di], ARK_TIME_Y, 24, 32, TFT_BLACK);
      applyDigitValue(di, targetValues[0]);
      timeOverridden = true;
      timeOverrideStart = millis();
      triggerBounce(di);
    }
  }

  // Multi-digit transitions
  if (breaking && fragTimer <= 0) {
    currentTarget++;
    if (currentTarget < numTargets) {
      int di = targetDigits[currentTarget];
      spawnDigitFragments(DIGIT_X[di], ARK_TIME_Y);
      tft.fillRect(DIGIT_X[di], ARK_TIME_Y, 24, 32, TFT_BLACK);
      applyDigitValue(di, targetValues[currentTarget]);
      triggerBounce(di);
    } else {
      breaking = false;
    }
  }

  // Physics
  updateBallPhysics();
  updatePaddle();
  updateBounce();
  updateFragments();

  // Draw date
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  char dateStr[20];
  snprintf(dateStr, sizeof(dateStr), "%s %02d.%02d.%04d",
           days[now.tm_wday], now.tm_mday, now.tm_mon + 1, now.tm_year + 1900);
  int dateW = strlen(dateStr) * 12;  // textSize 2 = ~12px per char
  tft.setCursor((SCREEN_W - dateW) / 2, ARK_DATE_Y);
  tft.print(dateStr);

  // Draw time, ball, paddle
  drawTime();
  drawBall();
  drawPaddle();
}
