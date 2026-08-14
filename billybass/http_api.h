/*
  Minimal HTTP control surface. Plain text, no HTML and no JSON library - this
  is for curl and for Home Assistant's RESTful sensor, not for a browser UI.

    GET /speak?text=hello+there    mouth the text; &delay=300 overrides spkdly
    GET /stop                      abandon the current utterance
    GET /status                    current telemetry and config
    GET /config?thresh=40          set one or more parameters, then persist
    GET /reset                     restore compiled-in defaults

  Parameter names match the MQTT config fields and the Preferences keys.
*/

#pragma once
#include <Arduino.h>

void httpApiBegin(uint16_t port);

// Call every loop(). Serves at most one request per call and never blocks
// longer than the read timeout.
void httpApiLoop();
