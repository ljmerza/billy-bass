#include "inputs.h"

// Long enough to swallow the contact bounce of a cheap tactile switch, short
// enough that the press still feels instant.
static const unsigned long DEBOUNCE_MS = 25;

static uint8_t btnPin = 0;
static bool begun = false;

// btnRaw is the last level read, btnState the debounced one. They differ only
// while a change is settling.
static bool btnRaw = false;
static bool btnState = false;
static unsigned long btnRawAt = 0;
static unsigned long btnDownAt = 0;
static uint16_t btnCount = 0;

// Set on the release edge and cleared by whoever reads it, so a press cannot be
// missed by a loop that happens to be busy on the pass it ended.
static bool btnReleased = false;
static unsigned long btnLastHeldMs = 0;

void inputsBegin(uint8_t buttonPin) {
  btnPin = buttonPin;
  pinMode(btnPin, INPUT_PULLUP);

  btnRawAt = millis();

  // Seed from the pin rather than assuming it starts idle. A button held
  // through boot then reads as already held, instead of arriving as a fresh
  // press one debounce window later.
  btnRaw = btnState = (digitalRead(btnPin) == LOW);
  if (btnState) btnDownAt = btnRawAt;

  begun = true;
}

void inputsLoop() {
  if (!begun) return;

  const unsigned long now = millis();

  // Every bounce restarts the settle window, so the level has to hold steady
  // for DEBOUNCE_MS before it counts. Subtraction throughout, so this keeps
  // working across the millis() rollover.
  const bool raw = (digitalRead(btnPin) == LOW);

  if (raw != btnRaw) {
    btnRaw = raw;
    btnRawAt = now;
  } else if (raw != btnState && now - btnRawAt >= DEBOUNCE_MS) {
    btnState = raw;
    if (btnState) {
      btnDownAt = now;
      btnCount++;
    } else {
      btnLastHeldMs = now - btnDownAt;
      btnReleased = true;
    }
  }
}

bool buttonDown() {
  return btnState;
}

unsigned long buttonHeldMs() {
  return btnState ? millis() - btnDownAt : 0;
}

uint16_t buttonPresses() {
  return btnCount;
}

bool buttonConsumeRelease(unsigned long& heldMs) {
  if (!btnReleased) return false;
  btnReleased = false;
  heldMs = btnLastHeldMs;
  return true;
}
