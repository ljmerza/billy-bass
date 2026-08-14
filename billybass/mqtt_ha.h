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

bool mqttConnected();
