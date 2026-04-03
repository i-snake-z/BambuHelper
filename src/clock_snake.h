#ifndef CLOCK_SNAKE_H
#define CLOCK_SNAKE_H

// Auto-playing Snake screensaver with HH:MM clock strip at the bottom.
// Call tickSnakeClock() from loop() while SCREEN_CLOCK is active.
void tickSnakeClock();

// Reset state (call when entering SCREEN_CLOCK).
void resetSnakeClock();

#endif // CLOCK_SNAKE_H
