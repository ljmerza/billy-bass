#include "FspTimer.h"
#include "swatchdog.h"

static const float TICK_HZ = 2.0f;            // 500ms supervisor resolution
static const uint32_t TRIPPED_MAGIC = 0xB1115A55;

// .noinit is NOLOAD in the UNO R4 linker script (fsp.ld:223), so this survives
// the warm reset that NVIC_SystemReset() performs. It does not survive a
// power cycle, which is exactly the distinction we want to report.
__attribute__((section(".noinit"))) static uint32_t trippedFlag;

static FspTimer swTimer;
static volatile uint32_t ticks = 0;
static volatile uint32_t tickLimit = 0;
static volatile bool suspended = false;
static bool running = false;
static bool tripped = false;

static void onTick(timer_callback_args_t* /*arg*/) {
  if (suspended || tickLimit == 0) return;

  if (++ticks >= tickLimit) {
    trippedFlag = TRIPPED_MAGIC;
    NVIC_SystemReset();
  }
}

bool swatchdogBegin(unsigned long timeoutMs) {
  tripped = (trippedFlag == TRIPPED_MAGIC);
  trippedFlag = 0;

  if (running || timeoutMs == 0) return running;

  uint8_t type;
  int8_t ch = FspTimer::get_available_timer(type);
  if (ch < 0) return false;                   // no free channel; caller reports

  tickLimit = (uint32_t)((timeoutMs * TICK_HZ) / 1000.0f);
  if (tickLimit == 0) tickLimit = 1;
  ticks = 0;

  if (!swTimer.begin(TIMER_MODE_PERIODIC, type, ch, TICK_HZ, 0.0f, onTick, nullptr)) return false;
  if (!swTimer.setup_overflow_irq()) return false;
  if (!swTimer.open()) return false;
  if (!swTimer.start()) return false;

  running = true;
  return true;
}

void swatchdogPet() {
  ticks = 0;
}

void swatchdogSuspend() {
  suspended = true;
}

void swatchdogResume() {
  ticks = 0;
  suspended = false;
}

bool swatchdogTripped() {
  return tripped;
}
