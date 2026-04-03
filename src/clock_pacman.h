#ifndef CLOCK_PACMAN_H
#define CLOCK_PACMAN_H

// Auto-playing Pac-Man screensaver with HH:MM clock strip at the bottom.
// Call tickPacManClock() from loop() while SCREEN_CLOCK is active.
void tickPacManClock();

// Reset state (call when entering SCREEN_CLOCK).
void resetPacManClock();

#endif // CLOCK_PACMAN_H
