#include <WiFiS3.h>       // must precede ArduinoOTA.h - it selects the WiFi variant
#include <ArduinoOTA.h>   // JAndrassy/ArduinoOTA; pulls in InternalStorageRenesas

#include "wifi_ota.h"
#include "arduino_secrets.h"
#include "swatchdog.h"
#include "status_display.h"

static const char* OTA_NAME = "billybass";   // mDNS name shown as the network port

// WiFiS3 has no non-blocking connect: CWifi::begin() busy-waits on status()
// until _timeout, which defaults to 10s. These bound the stall instead.
static const unsigned long FIRST_CONNECT_MS = 8000;
static const unsigned long RECONNECT_MS     = 4000;
static const unsigned long RETRY_MS         = 30000;  // gap between attempts

// Both of these are modem round-trips over the UART to the ESP32-S3, and
// status() adds a delay(1). Running either every loop() pass would throttle
// the sound sampling, so they are scheduled rather than free-running.
static const unsigned long POLL_MS   = 50;
static const unsigned long STATUS_MS = 5000;

static bool updating = false;
static bool connected = false;
static bool otaStarted = false;
static bool firstConnect = true;
static unsigned long lastAttempt = 0;
static unsigned long lastPoll = 0;
static unsigned long lastStatus = 0;
static void (*haltHook)() = nullptr;

static void halt() {
  if (haltHook) haltHook();
}

static void onOtaStart() {
  updating = true;
  halt();                                          // motors off for the transfer
  swatchdogSuspend();                              // poll() blocks for the whole upload
  statusDisplay(STATUS_OTA);
  if (DEBUG) Serial.println("OTA: upload started");
}

static void onOtaError(int code, const char* msg) {
  updating = false;
  halt();                                          // torn transfer, leave it limp
  swatchdogResume();
  statusDisplay(STATUS_ONLINE);
  if (DEBUG) {
    Serial.print("OTA error ");
    Serial.print(code);
    Serial.print(": ");
    Serial.println(msg);
  }
}

static void startOtaService() {
  ArduinoOTA.onStart(onOtaStart);
  ArduinoOTA.beforeApply(halt);                    // last chance before the reset
  ArduinoOTA.onError(onOtaError);
  ArduinoOTA.begin(WiFi.localIP(), OTA_NAME, SECRET_OTA_PASS, InternalStorage);
  otaStarted = true;

  if (DEBUG) {
    Serial.print("OTA ready at ");
    Serial.println(WiFi.localIP());
  }
}

static void stopOtaService() {
  if (otaStarted) {
    ArduinoOTA.end();                              // release socket + mDNS
    otaStarted = false;
  }
}

static void tryConnect(unsigned long timeoutMs) {
  // WiFi.begin() busy-waits for up to timeoutMs. Frozen-and-limp is safe,
  // frozen-and-driven is not, so stop the motors before stalling.
  halt();
  swatchdogPet();                                  // full window for the stall

  lastAttempt = millis();
  statusDisplay(STATUS_CONNECTING);
  if (DEBUG) Serial.println("WiFi: connecting...");

  // Must precede begin() - this is the name sent in the DHCP request, which is
  // what the router shows in its client list.
  WiFi.setHostname(OTA_NAME);
  WiFi.setTimeout(timeoutMs);
  connected = (WiFi.begin(SECRET_SSID, SECRET_PASS) == WL_CONNECTED);
  lastStatus = millis();
  swatchdogPet();

  if (connected) {
    startOtaService();
    statusDisplay(STATUS_ONLINE);
    if (firstConnect) {
      firstConnect = false;
      statusDisplayShowOctet(WiFi.localIP()[3]);   // blocking; boot-time only
      statusDisplay(STATUS_ONLINE);
    }
  } else {
    statusDisplay(STATUS_OFFLINE);
    if (DEBUG) Serial.println("WiFi: connect failed");
  }
}

void wifiOtaSetup(void (*onHalt)()) {
  haltHook = onHalt;

  if (WiFi.status() == WL_NO_MODULE) {
    if (DEBUG) Serial.println("WiFi: module not found, OTA disabled");
    return;
  }

  tryConnect(FIRST_CONNECT_MS);
}

void wifiOtaLoop(bool safeToBlock) {
  unsigned long now = millis();

  if (!connected) {
    if (safeToBlock && now - lastAttempt >= RETRY_MS) tryConnect(RECONNECT_MS);
    return;
  }

  if (now - lastPoll >= POLL_MS) {
    lastPoll = now;
    ArduinoOTA.poll();                             // blocks for the whole upload
  }

  if (now - lastStatus >= STATUS_MS) {
    lastStatus = now;
    if (WiFi.status() != WL_CONNECTED) {
      if (DEBUG) Serial.println("WiFi: connection lost");
      stopOtaService();
      statusDisplay(STATUS_OFFLINE);
      connected = false;
      lastAttempt = now;                           // full retry gap before stalling
    }
  }
}

bool wifiOtaUpdating() {
  return updating;
}

bool wifiOtaConnected() {
  return connected;
}
