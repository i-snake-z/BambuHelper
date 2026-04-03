#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

void initButton();
void pollButton();                     // call once per loop to update debounced state
bool wasButtonPressed();               // returns true once per short press (edge-detected, debounced)

#endif // BUTTON_H
