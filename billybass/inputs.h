/*
  The momentary button on D2.

  Read by polling from loop(), not by interrupt. loop() runs far faster than a
  finger moves, and an ISR would fire in the middle of the I2C transactions the
  motor shield needs - polling costs one digitalRead a pass and has none of
  that to go wrong.

  This module only reports what the pin is doing. Nothing here decides what a
  press means; that belongs to the sketch.
*/

#pragma once
#include <Arduino.h>

// buttonPin is read with the internal pull-up, so wire the switch between the
// pin and ground: the pin idles high and a press pulls it low. Feeding the
// button from 5V instead leaves the pin floating whenever it is not pressed,
// and a floating input picks up mains hum and reads pressed at random - the
// RA4M1 has internal pull-ups but no pull-downs, so there is no software fix
// for that wiring.
void inputsBegin(uint8_t buttonPin);

// Call every loop().
void inputsLoop();

bool buttonDown();                 // debounced; true for as long as it is held
unsigned long buttonHeldMs();      // length of the press so far, 0 when up
uint16_t buttonPresses();          // presses since boot

// True exactly once, on the loop where a press ends, with heldMs filled in.
// Whoever consumes it owns that press.
//
// The gesture is classified on release rather than on the way down, so one
// press produces exactly one event and an automation on a short press never
// also fires partway through a long one. The cost is that the event lands when
// you let go - imperceptible on a short press, and the reason a long press
// gives no feedback at the moment it crosses the threshold.
bool buttonConsumeRelease(unsigned long& heldMs);
