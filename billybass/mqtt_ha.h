/*
  MQTT client with Home Assistant auto-discovery.

  Publishes telemetry the fish already computes, and accepts config changes on
  the same keys the HTTP endpoint uses. On connect it publishes retained
  discovery documents, so the device and its entities appear in Home Assistant
  without any YAML.

  Entirely optional: leave SECRET_MQTT_HOST empty and this compiles in but never
  connects, costing nothing at runtime.
*/

#pragma once
#include <Arduino.h>

void mqttBegin();

// Call every loop(). Reconnects on a timer and publishes telemetry on an
// interval; never blocks longer than one connect attempt.
void mqttLoop();

// Publishes one button event immediately, outside the telemetry interval. A
// press lasts a fraction of the gap between scheduled publishes, so folding it
// into the periodic snapshot would miss it almost every time.
//
// eventType must be one of the strings the entity declares in event_types.
// Dropped silently when the broker is not connected: a press is a moment, and
// there is nothing useful to deliver about it later.
void mqttPublishButton(const char* eventType);

bool mqttConnected();
