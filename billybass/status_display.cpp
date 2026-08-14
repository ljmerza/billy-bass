#include <Arduino_LED_Matrix.h>
#include "status_display.h"

static ArduinoLEDMatrix matrix;
static StatusState current = STATUS_BOOTING;
static bool started = false;

// 12x8 glyphs, packed as loadFrame() expects: pixel i (row*12 + col, row 0 at
// the top) lives in word i/32 at bit 31-(i%32).
static const uint32_t GLYPH_BOOT[3]       = { 0x00000010, 0x82942941, 0x08000000 };
static const uint32_t GLYPH_CONNECTING[3] = { 0x00000000, 0x02482480, 0x00000000 };
static const uint32_t GLYPH_ONLINE[3]     = { 0x0001F820, 0x40600900, 0x00060060 };
static const uint32_t GLYPH_OFFLINE[3]    = { 0x00020410, 0x80900600, 0x90108204 };
static const uint32_t GLYPH_OTA[3]        = { 0x06006006, 0x00603F81, 0xF00E0040 };
static const uint32_t GLYPH_SAFE[3]       = { 0x06006006, 0x00600600, 0x00060060 };

// 3x5 digit font, one nibble-wide row per entry. Enough to read the last octet
// of the IP off the fish without a serial cable.
static const uint8_t DIGITS[10][5] = {
  {0b111,0b101,0b101,0b101,0b111}, // 0
  {0b010,0b110,0b010,0b010,0b111}, // 1
  {0b111,0b001,0b111,0b100,0b111}, // 2
  {0b111,0b001,0b111,0b001,0b111}, // 3
  {0b101,0b101,0b111,0b001,0b001}, // 4
  {0b111,0b100,0b111,0b001,0b111}, // 5
  {0b111,0b100,0b111,0b101,0b111}, // 6
  {0b111,0b001,0b001,0b001,0b001}, // 7
  {0b111,0b101,0b111,0b101,0b111}, // 8
  {0b111,0b101,0b111,0b001,0b111}  // 9
};

static void setPixel(uint32_t frame[3], uint8_t row, uint8_t col) {
  if (row > 7 || col > 11) return;
  uint8_t idx = row * 12 + col;
  frame[idx / 32] |= (uint32_t)1 << (31 - (idx % 32));
}

static void drawDigit(uint32_t frame[3], uint8_t digit, uint8_t colOffset) {
  if (digit > 9) return;
  for (uint8_t r = 0; r < 5; r++) {
    for (uint8_t c = 0; c < 3; c++) {
      if (DIGITS[digit][r] & (1 << (2 - c))) setPixel(frame, r + 1, colOffset + c);
    }
  }
}

void statusDisplayBegin() {
  matrix.begin();
  started = true;
  matrix.loadFrame(GLYPH_BOOT);
  current = STATUS_BOOTING;
}

void statusDisplay(StatusState state) {
  if (!started || state == current) return;
  current = state;

  switch (state) {
    case STATUS_BOOTING:    matrix.loadFrame(GLYPH_BOOT);       break;
    case STATUS_CONNECTING: matrix.loadFrame(GLYPH_CONNECTING); break;
    case STATUS_ONLINE:     matrix.loadFrame(GLYPH_ONLINE);     break;
    case STATUS_OFFLINE:    matrix.loadFrame(GLYPH_OFFLINE);    break;
    case STATUS_OTA:        matrix.loadFrame(GLYPH_OTA);        break;
    case STATUS_SAFE_MODE:  matrix.loadFrame(GLYPH_SAFE);       break;
  }
}

void statusDisplayShowOctet(uint8_t octet) {
  if (!started) return;

  uint32_t frame[3] = { 0, 0, 0 };
  drawDigit(frame, (octet / 100) % 10, 0);
  drawDigit(frame, (octet / 10) % 10,  4);
  drawDigit(frame, octet % 10,         8);

  matrix.loadFrame(frame);
  delay(2500);                    // boot-time one-shot; call before the watchdog
  current = STATUS_BOOTING;       // force the next statusDisplay() to redraw
}
