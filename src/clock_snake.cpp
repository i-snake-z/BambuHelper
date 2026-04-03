/*
 * Snake animated clock screensaver for BambuHelper
 *
 * An auto-piloted Snake game fills the screen; the current time (HH:MM)
 * is shown in a 40-px strip at the bottom — redrawn only on minute change.
 *
 * Grid: 8 px per cell, 30 cols × 25 rows (240×200 px play area).
 * AI:   greedy - always tries to move toward food; avoids walls and its own
 *       body. Falls back to any safe direction if the preferred path is
 *       blocked. On collision the board clears and a new game starts.
 */

#include "clock_snake.h"
#include "config.h"
#include "settings.h"
#include <TFT_eSPI.h>
#include <time.h>

extern TFT_eSPI tft;

// ---- Grid dimensions -------------------------------------------------------
#define SN_CELL      10
#define SN_TIME_H    56                                    // bottom strip (px)
#define SN_COLS      (SCREEN_W / SN_CELL)                 // 24 columns
#define SN_ROWS      ((SCREEN_H - SN_TIME_H) / SN_CELL)  // 18 rows (180 px board)
#define SN_MAX_LEN   220                                   // body array cap
#define SN_UPDATE_MS 130                                   // ~7.7 fps

// ---- Colors ----------------------------------------------------------------
#define SN_CLR_HEAD  0x07E0   // bright green
#define SN_CLR_BODY  0x03E0   // medium green
#define SN_CLR_FOOD  0xF800   // red
#define SN_CLR_BG    0x0000   // black

// ---- Direction -------------------------------------------------------------
enum SnDir : uint8_t { SN_UP = 0, SN_DOWN, SN_LEFT, SN_RIGHT };

static const int8_t DX[4] = {  0,  0, -1,  1 };   // UP DOWN LEFT RIGHT
static const int8_t DY[4] = { -1,  1,  0,  0 };
static const SnDir  REV[4] = { SN_DOWN, SN_UP, SN_RIGHT, SN_LEFT };

// ---- State -----------------------------------------------------------------
struct SCell { int8_t x, y; };

static SCell  body[SN_MAX_LEN]; // body[0] = head, body[sLen-1] = tail
static int    sLen = 0;
static SnDir  sDir = SN_RIGHT;
static SCell  sFd;              // current food position
static bool   grid[SN_ROWS][SN_COLS]; // occupancy (true = body cell)
static bool   snkInit    = false;
static unsigned long lastStepMs = 0;

// Time overlay state
static int prevSMin  = -1;
static int prevSHour = -1;

// ---- Draw helpers ----------------------------------------------------------

static void drawCell(int8_t cx, int8_t cy, uint16_t color) {
    // 4×4 block inset 1 px inside the 6×6 cell — leaves a 1-px grid gap
    tft.fillRect(cx * SN_CELL + 1, cy * SN_CELL + 1, SN_CELL - 2, SN_CELL - 2, color);
}

static void eraseCell(int8_t cx, int8_t cy) {
    tft.fillRect(cx * SN_CELL, cy * SN_CELL, SN_CELL, SN_CELL, SN_CLR_BG);
}

static void drawFood() {
    int px = sFd.x * SN_CELL;
    int py = sFd.y * SN_CELL;
    tft.fillRect(px + 1, py + 1, SN_CELL - 2, SN_CELL - 2, SN_CLR_FOOD);
    // Tiny highlight pixel for style
    tft.drawPixel(px + 2, py + 2, 0xFFC0);
}

// ---- Time overlay ----------------------------------------------------------

static void drawSnakeTime(bool force = false) {
    struct tm now;
    if (!getLocalTime(&now, 0)) return;

    if (!force && now.tm_min == prevSMin && now.tm_hour == prevSHour) return;

    int stripY = SN_ROWS * SN_CELL;
    tft.fillRect(0, stripY, SCREEN_W, SN_TIME_H, SN_CLR_BG);

    char buf[8];
    if (netSettings.use24h) {
        snprintf(buf, sizeof(buf), "%02d:%02d", now.tm_hour, now.tm_min);
    } else {
        int h = now.tm_hour % 12;
        if (h == 0) h = 12;
        snprintf(buf, sizeof(buf), "%2d:%02d", h, now.tm_min);
    }

    tft.setTextFont(6);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, SN_CLR_BG);
    tft.drawString(buf, SCREEN_W / 2, stripY + SN_TIME_H / 2);

    prevSMin  = now.tm_min;
    prevSHour = now.tm_hour;
}

// ---- Food placement --------------------------------------------------------

static void placeFood() {
    SCell c;
    int tries = 0;
    do {
        c.x = (int8_t)random(0, SN_COLS);
        c.y = (int8_t)random(0, SN_ROWS);
        tries++;
    } while (grid[c.y][c.x] && tries < 600);
    sFd = c;
}

// ---- Initialise a new game -------------------------------------------------

static void reInitSnake() {
    memset(grid, 0, sizeof(grid));
    sLen = 4;
    int8_t sx = SN_COLS / 2;
    int8_t sy = SN_ROWS / 2;
    for (int i = 0; i < sLen; i++) {
        body[i] = { (int8_t)(sx - i), sy };
        grid[sy][sx - i] = true;
    }
    sDir = SN_RIGHT;
    placeFood();
}

// ---- AI: choose next direction ---------------------------------------------
// Priority: directions that reduce Manhattan distance to food come first,
// then the remaining two. Never reverses into self. Falls back to curDir
// when completely trapped (will die next step and trigger a reset).

static SnDir pickDir() {
    int8_t hx = body[0].x, hy = body[0].y;
    int8_t fx = sFd.x,     fy = sFd.y;

    SnDir pref[4];
    int   np = 0;

    // Food-approach directions first
    if (fx > hx) pref[np++] = SN_RIGHT;
    if (fx < hx) pref[np++] = SN_LEFT;
    if (fy > hy) pref[np++] = SN_DOWN;
    if (fy < hy) pref[np++] = SN_UP;

    // Fill remaining directions
    static const SnDir all[4] = { SN_LEFT, SN_RIGHT, SN_UP, SN_DOWN };
    for (int i = 0; i < 4; i++) {
        bool seen = false;
        for (int j = 0; j < np; j++) if (pref[j] == all[i]) { seen = true; break; }
        if (!seen) pref[np++] = all[i];
    }

    SnDir forbidden = REV[(int)sDir];
    for (int i = 0; i < 4; i++) {
        SnDir d = pref[i];
        if (d == forbidden) continue;
        int8_t nx = hx + DX[(int)d];
        int8_t ny = hy + DY[(int)d];
        if (nx >= 0 && nx < SN_COLS && ny >= 0 && ny < SN_ROWS && !grid[ny][nx])
            return d;
    }
    return sDir;  // trapped — will die on next step
}

// ---- Public API ------------------------------------------------------------

void resetSnakeClock() {
    snkInit = false;
}

void tickSnakeClock() {
    if (!dpSettings.showClockAfterFinish && !dpSettings.keepDisplayOn) return;
    if (!dispSettings.snakeClock) return;

    struct tm now;
    if (!getLocalTime(&now, 0)) return;

    // First-time initialisation
    if (!snkInit) {
        tft.fillScreen(SN_CLR_BG);
        reInitSnake();
        for (int i = 1; i < sLen; i++) drawCell(body[i].x, body[i].y, SN_CLR_BODY);
        drawCell(body[0].x, body[0].y, SN_CLR_HEAD);
        drawFood();
        prevSMin  = -1;
        prevSHour = -1;
        snkInit   = true;
        lastStepMs = millis();
        drawSnakeTime(true);  // draw time immediately on init
        return;
    }

    // Time strip: redraw only on minute change
    drawSnakeTime();

    // Snake step throttle
    unsigned long ms = millis();
    if (ms - lastStepMs < SN_UPDATE_MS) return;
    lastStepMs = ms;

    sDir = pickDir();
    int8_t nx = body[0].x + DX[(int)sDir];
    int8_t ny = body[0].y + DY[(int)sDir];

    // Collision: wall or self
    bool dead = (nx < 0 || nx >= SN_COLS || ny < 0 || ny >= SN_ROWS || grid[ny][nx]);
    if (dead) {
        tft.fillRect(0, 0, SCREEN_W, SN_ROWS * SN_CELL, SN_CLR_BG);
        reInitSnake();
        for (int i = 1; i < sLen; i++) drawCell(body[i].x, body[i].y, SN_CLR_BODY);
        drawCell(body[0].x, body[0].y, SN_CLR_HEAD);
        drawFood();
        return;
    }

    bool ate     = (nx == sFd.x && ny == sFd.y);
    bool growing = (ate && sLen < SN_MAX_LEN);

    // Remove tail from display and grid whenever the snake is not growing
    if (!growing) {
        SCell tail = body[sLen - 1];
        eraseCell(tail.x, tail.y);
        grid[tail.y][tail.x] = false;
    }

    // Extend body array when growing
    if (growing) sLen++;

    // Shift body array so body[0] can hold the new head
    memmove(&body[1], &body[0], (sLen - 1) * sizeof(SCell));
    body[0] = { nx, ny };
    grid[ny][nx] = true;

    // Old head (now neck) switches to body color; new head is drawn bright
    if (sLen > 1) drawCell(body[1].x, body[1].y, SN_CLR_BODY);
    drawCell(body[0].x, body[0].y, SN_CLR_HEAD);

    if (ate) {
        placeFood();
        drawFood();
    }
}
