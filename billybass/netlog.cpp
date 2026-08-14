#include <WiFiS3.h>
#include "netlog.h"

static WiFiUDP udp;
static uint16_t logPort = 0;
static IPAddress broadcast;
static bool started = false;

void netlogBegin(uint16_t port) {
  if (started || WiFi.status() != WL_CONNECTED) return;

  logPort = port;

  // Subnet broadcast rather than a configured collector: no listener address to
  // keep in sync, and any machine on the LAN can just listen. Cached here
  // because WiFi.localIP() is a modem round-trip, not a local read.
  broadcast = WiFi.localIP();
  broadcast[3] = 255;

  udp.begin(port);
  started = true;
}

void netlogPrint(const String& line) {
  if (!started) return;

  udp.beginPacket(broadcast, logPort);
  udp.print(line);
  udp.print('\n');
  udp.endPacket();
}
