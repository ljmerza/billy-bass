#include <WiFiS3.h>
#include <PubSubClient.h>

#include "mqtt_ha.h"
#include "arduino_secrets.h"
#include "config.h"
#include "telemetry.h"
#include "swatchdog.h"
#include "speech.h"

static const char* DEVICE_ID   = "billybass";
static const char* T_AVAIL     = "billybass/status";
static const char* T_TELEMETRY = "billybass/telemetry";
static const char* T_CONFIG    = "billybass/config";
static const char* T_CONFIG_SET = "billybass/config/set";
static const char* T_SPEAK     = "billybass/speak";
static const char* T_SPEAK_STATE = "billybass/speak/state";
static const char* T_BUTTON    = "billybass/button";

static const unsigned long RECONNECT_MS = 15000;
static const unsigned long PUBLISH_MS   = 2000;
static const uint16_t BUFFER_SIZE = 512;   // discovery payloads exceed the 256 default

static WiFiClient net;
static PubSubClient client(net);
static unsigned long lastAttempt = 0;
static unsigned long lastPublish = 0;
static bool enabled = false;

static void publishConfigState() {
  String body;
  body.reserve(96);
  configFormat(body);
  client.publish(T_CONFIG, body.c_str(), true);
}

static void onMessage(char* topic, byte* payload, unsigned int length) {
  String msg;
  msg.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  if (!strcmp(topic, T_SPEAK)) {
    // Payload is the text itself. Timing comes from the persisted config, so
    // Home Assistant can send a bare string.
    if (speechSay(msg.c_str(), config().speakDelayMs)) {
      client.publish(T_SPEAK_STATE, msg.c_str(), true);
    }
    return;
  }

  if (strcmp(topic, T_CONFIG_SET) != 0) return;

  // Accepts "key=value" or several separated by spaces, matching the HTTP form.
  int pos = 0;
  bool any = false;

  while (pos < (int)msg.length()) {
    int sp = msg.indexOf(' ', pos);
    if (sp < 0) sp = msg.length();

    int eq = msg.indexOf('=', pos);
    if (eq > 0 && eq < sp) {
      String key = msg.substring(pos, eq);
      if (configSet(key.c_str(), msg.substring(eq + 1, sp).toInt())) any = true;
    }
    pos = sp + 1;
  }

  if (any) {
    configSave();
    publishConfigState();
  }
}

// One retained discovery document per entity. The device block is repeated in
// each so Home Assistant groups them under a single device.
static void publishDiscovery() {
  String doc;
  doc.reserve(BUFFER_SIZE);

  const char* device =
    "\"dev\":{\"ids\":[\"billybass\"],\"name\":\"Billy Bass\","
    "\"mdl\":\"UNO R4 WiFi\",\"mf\":\"Big Mouth\"}";

  doc = "{\"name\":\"Sound Level\",\"uniq_id\":\"billybass_sound\",";
  doc += "\"stat_t\":\"billybass/telemetry\",\"avty_t\":\"billybass/status\",";
  doc += "\"val_tpl\":\"{{ value_json.sound }}\",\"unit_of_meas\":\"lvl\",";
  doc += device;
  doc += "}";
  client.publish("homeassistant/sensor/billybass/sound/config", doc.c_str(), true);

  doc = "{\"name\":\"Mouth Speed\",\"uniq_id\":\"billybass_mouth\",";
  doc += "\"stat_t\":\"billybass/telemetry\",\"avty_t\":\"billybass/status\",";
  doc += "\"val_tpl\":\"{{ value_json.mouth }}\",";
  doc += device;
  doc += "}";
  client.publish("homeassistant/sensor/billybass/mouth/config", doc.c_str(), true);

  doc = "{\"name\":\"Head\",\"uniq_id\":\"billybass_head\",";
  doc += "\"stat_t\":\"billybass/telemetry\",\"avty_t\":\"billybass/status\",";
  doc += "\"val_tpl\":\"{{ value_json.head }}\",\"dev_cla\":\"motion\",";
  doc += device;
  doc += "}";
  client.publish("homeassistant/binary_sensor/billybass/head/config", doc.c_str(), true);

  doc = "{\"name\":\"Tail\",\"uniq_id\":\"billybass_tail\",";
  doc += "\"stat_t\":\"billybass/telemetry\",\"avty_t\":\"billybass/status\",";
  doc += "\"val_tpl\":\"{{ value_json.tail }}\",\"dev_cla\":\"motion\",";
  doc += device;
  doc += "}";
  client.publish("homeassistant/binary_sensor/billybass/tail/config", doc.c_str(), true);

  // An event entity rather than a binary_sensor. A press is a moment, not a
  // state: it is over in a fraction of the telemetry interval, so a sensor
  // sampled on that interval would miss nearly every one. This entity is fed by
  // its own immediate publish instead, shows up in Home Assistant with history
  // so presses can be confirmed before any automation exists, and is pickable
  // as an automation trigger on its event_type.
  //
  // "event_types" is spelled out rather than abbreviated - the abbreviations
  // used above are ones this code already relies on, and there is no reason to
  // guess at one here.
  doc = "{\"name\":\"Button\",\"uniq_id\":\"billybass_button\",";
  doc += "\"stat_t\":\"billybass/button\",\"avty_t\":\"billybass/status\",";
  doc += "\"dev_cla\":\"button\",";
  doc += "\"event_types\":[\"press\",\"long_press\"],";
  doc += device;
  doc += "}";
  client.publish("homeassistant/event/billybass/button/config", doc.c_str(), true);

  // A text entity, so Home Assistant renders a box you can type a line into and
  // the fish mouths it. Also the target for automations and notify actions.
  doc = "{\"name\":\"Say\",\"uniq_id\":\"billybass_say\",";
  doc += "\"cmd_t\":\"billybass/speak\",\"stat_t\":\"billybass/speak/state\",";
  doc += "\"avty_t\":\"billybass/status\",\"max\":240,";
  doc += device;
  doc += "}";
  client.publish("homeassistant/text/billybass/say/config", doc.c_str(), true);
}

static bool reconnect() {
  lastAttempt = millis();
  swatchdogPet();     // TCP connect to an unreachable broker can stall for seconds

  bool ok = client.connect(
    DEVICE_ID,
    strlen(SECRET_MQTT_USER) ? SECRET_MQTT_USER : nullptr,
    strlen(SECRET_MQTT_PASS) ? SECRET_MQTT_PASS : nullptr,
    T_AVAIL, 0, true, "offline");

  if (!ok) return false;

  client.publish(T_AVAIL, "online", true);
  client.subscribe(T_CONFIG_SET);
  client.subscribe(T_SPEAK);
  publishDiscovery();
  publishConfigState();
  return true;
}

void mqttBegin() {
  enabled = (strlen(SECRET_MQTT_HOST) > 0);
  if (!enabled) return;

  client.setServer(SECRET_MQTT_HOST, SECRET_MQTT_PORT);
  client.setBufferSize(BUFFER_SIZE);
  client.setCallback(onMessage);
}

void mqttLoop() {
  if (!enabled || WiFi.status() != WL_CONNECTED) return;

  if (!client.connected()) {
    if (millis() - lastAttempt >= RECONNECT_MS) reconnect();
    return;
  }

  client.loop();

  unsigned long now = millis();
  if (now - lastPublish < PUBLISH_MS) return;
  lastPublish = now;

  const Telemetry& t = telemetry();
  String body;
  body.reserve(96);
  body += "{\"sound\":";  body += t.soundLevel;
  body += ",\"mouth\":";  body += t.mouthSpeed;
  body += ",\"head\":\""; body += (t.headActive ? "ON" : "OFF");
  body += "\",\"tail\":\""; body += (t.tailActive ? "ON" : "OFF");
  body += "\",\"uptime\":"; body += (now / 1000);
  body += "}";

  client.publish(T_TELEMETRY, body.c_str());
}

void mqttPublishButton(const char* eventType) {
  if (!enabled || !client.connected()) return;

  // Not retained: Home Assistant discards replayed retained messages on an
  // event entity, so retaining one would buy nothing and only leave a stale
  // press sitting on the broker.
  String body;
  body.reserve(40);
  body = "{\"event_type\":\"";
  body += eventType;
  body += "\"}";

  client.publish(T_BUTTON, body.c_str());
}

bool mqttConnected() {
  return enabled && client.connected();
}
