#include "speech.h"
#include "config.h"

// Mouth opening per vowel, as a percentage of the sketch's full sound range.
// Open vowels move the jaw further, which is most of what sells the effect.
struct VowelShape {
  char c;
  uint8_t levelPct;
  uint16_t openMs;
};

static const VowelShape VOWELS[] = {
  { 'a', 94, 130 },
  { 'o', 89, 125 },
  { 'e', 72, 110 },
  { 'u', 67, 105 },
  { 'i', 61, 100 },
  { 'y', 61, 100 }
};
static const uint8_t VOWEL_COUNT = sizeof(VOWELS) / sizeof(VOWELS[0]);

// Digits and anything else pronounceable but not vowel-shaped.
static const uint8_t FALLBACK_LEVEL_PCT = 50;
static const uint16_t FALLBACK_OPEN_MS  = 95;

static const uint16_t CLOSE_MS    = 70;         // jaw shut between syllables
static const uint16_t WORD_GAP_MS = 60;
static const uint16_t COMMA_MS    = 200;
static const uint16_t SENTENCE_MS = 380;

enum Phase { IDLE, DELAY, OPEN, CLOSE, PAUSE };

static char buf[SPEECH_MAX_LEN + 1];
static size_t pos = 0;
static Phase phase = IDLE;
static unsigned long phaseUntil = 0;
static int level = 0;
static int fullScale = 180;
static uint8_t syllablesInWord = 0;

static bool isVowel(char c) {
  return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y';
}

static bool isLetter(char c) {
  return (c >= 'a' && c <= 'z');
}

// Speaking rate is a percentage: 200 means twice as fast, so half the duration.
static unsigned long scaled(unsigned long ms) {
  int rate = config().speakRatePct;
  if (rate < 50) rate = 50;
  return (ms * 100UL) / (unsigned long)rate;
}

static void enter(Phase p, unsigned long durationMs, int lvl) {
  phase = p;
  level = lvl;
  phaseUntil = millis() + durationMs;
}

static const VowelShape* shapeFor(char c) {
  for (uint8_t i = 0; i < VOWEL_COUNT; i++) {
    if (VOWELS[i].c == c) return &VOWELS[i];
  }
  return nullptr;
}

// Walks forward to the next syllable, emitting pauses for punctuation and word
// breaks along the way. Sets the next phase and returns.
static void nextSyllable() {
  while (pos < SPEECH_MAX_LEN && buf[pos] != '\0') {
    char c = buf[pos];

    if (isLetter(c)) {
      if (!isVowel(c)) { pos++; continue; }          // consonants are just onset

      // Collect the whole vowel run; its first character sets the shape.
      char nucleus = c;
      size_t runStart = pos;
      while (pos < SPEECH_MAX_LEN && isVowel(buf[pos])) pos++;

      // Silent trailing 'e': a lone 'e' ending a word that already has a
      // syllable is not pronounced. "have" is one syllable, not two.
      bool endsWord = (pos >= SPEECH_MAX_LEN) || !isLetter(buf[pos]);
      bool loneE = (nucleus == 'e') && ((pos - runStart) == 1);

      if (loneE && endsWord && syllablesInWord > 0) {
        // Exception: "-le" after a consonant is syllabic and does get a beat.
        // "cycle" and "little" are two syllables, "have" is still one.
        char prev  = (runStart > 0) ? buf[runStart - 1] : '\0';
        char prev2 = (runStart > 1) ? buf[runStart - 2] : '\0';
        bool syllabicLe = (prev == 'l') && isLetter(prev2) && !isVowel(prev2);

        if (!syllabicLe) continue;
      }

      // Silent "-ed" suffix: "jumped" and "opened" are one and two syllables,
      // not two and three. The exception is a stem ending in t or d, where the
      // suffix is voiced - "wanted", "needed".
      if (loneE && syllablesInWord > 0 && buf[pos] == 'd' &&
          ((pos + 1) >= SPEECH_MAX_LEN || !isLetter(buf[pos + 1]))) {
        char stemEnd = (runStart > 0) ? buf[runStart - 1] : '\0';
        if (stemEnd != 't' && stemEnd != 'd') {
          pos++;                    // consume the d along with the silent e
          continue;
        }
      }

      syllablesInWord++;
      const VowelShape* s = shapeFor(nucleus);
      uint8_t pct = s ? s->levelPct : FALLBACK_LEVEL_PCT;
      uint16_t openMs = s ? s->openMs : FALLBACK_OPEN_MS;

      enter(OPEN, scaled(openMs), (int)((long)fullScale * pct / 100));
      return;
    }

    // Digits are spoken, so give each one a syllable. Without this a phone
    // number or a temperature reads as silence.
    if (c >= '0' && c <= '9') {
      pos++;
      syllablesInWord++;
      enter(OPEN, scaled(FALLBACK_OPEN_MS),
            (int)((long)fullScale * FALLBACK_LEVEL_PCT / 100));
      return;
    }

    // Not a letter: punctuation and whitespace become silence.
    pos++;

    if (c == ',' || c == ';' || c == ':') {
      syllablesInWord = 0;
      enter(PAUSE, scaled(COMMA_MS), 0);
      return;
    }
    if (c == '.' || c == '!' || c == '?') {
      syllablesInWord = 0;
      enter(PAUSE, scaled(SENTENCE_MS), 0);
      return;
    }
    if (c == ' ' || c == '\t' || c == '\n') {
      if (syllablesInWord == 0) continue;            // collapse runs of spaces
      syllablesInWord = 0;
      enter(PAUSE, scaled(WORD_GAP_MS), 0);
      return;
    }
  }

  speechStop();
}

void speechBegin(int maxLevel) {
  fullScale = maxLevel;
  speechStop();
}

bool speechSay(const char* text, unsigned long startDelayMs) {
  if (!text) return false;

  size_t len = strlen(text);
  if (len == 0 || len > SPEECH_MAX_LEN) return false;

  // Lowercase once here so the scanner never has to case-fold.
  for (size_t i = 0; i < len; i++) {
    char c = text[i];
    buf[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
  }
  buf[len] = '\0';

  pos = 0;
  syllablesInWord = 0;

  if (startDelayMs > 0) enter(DELAY, startDelayMs, 0);
  else nextSyllable();

  return true;
}

void speechStop() {
  phase = IDLE;
  level = 0;
  pos = 0;
  syllablesInWord = 0;
}

bool speechActive() {
  return phase != IDLE;
}

int speechLevel() {
  return level;
}

void speechLoop() {
  if (phase == IDLE) return;

  // Subtraction so this survives the millis() rollover.
  if ((long)(millis() - phaseUntil) < 0) return;

  switch (phase) {
    case OPEN:
      enter(CLOSE, scaled(CLOSE_MS), 0);
      break;

    case DELAY:
    case CLOSE:
    case PAUSE:
      nextSyllable();
      break;

    case IDLE:
      break;
  }
}
