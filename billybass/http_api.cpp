#include <WiFiS3.h>
#include <Wire.h>
#include "http_api.h"
#include "config.h"
#include "telemetry.h"
#include "speech.h"
#include "motortest.h"

static const unsigned long READ_TIMEOUT_MS = 500;
static const size_t MAX_REQUEST_LINE = 200;

static WiFiServer server;
static bool started = false;

static void sendText(WiFiClient& client, const char* status, const String& body) {
  client.print("HTTP/1.1 ");
  client.println(status);
  client.println("Content-Type: text/plain");
  client.print("Content-Length: ");
  client.println(body.length());
  client.println("Connection: close");
  client.println();
  client.print(body);
}

static int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Percent-decoding, so typed text with spaces and punctuation survives the
// query string. '+' means space, per form encoding.
static String urlDecode(const String& in) {
  String out;
  out.reserve(in.length());

  for (unsigned int i = 0; i < in.length(); i++) {
    char c = in[i];

    if (c == '+') {
      out += ' ';
    } else if (c == '%' && i + 2 < in.length()) {
      int hi = hexVal(in[i + 1]);
      int lo = hexVal(in[i + 2]);
      if (hi < 0 || lo < 0) { out += c; continue; }
      out += (char)((hi << 4) | lo);
      i += 2;
    } else {
      out += c;
    }
  }

  return out;
}

// Returns the raw value of one query parameter, or an empty string.
static String queryValue(const String& query, const char* key) {
  String needle = String(key) + "=";
  int at = query.startsWith(needle) ? 0 : query.indexOf("&" + needle);
  if (at < 0) return String();
  if (at > 0) at++;                       // step past the '&'

  int from = at + needle.length();
  int amp = query.indexOf('&', from);
  return query.substring(from, amp < 0 ? query.length() : amp);
}

// Applies every key=value pair in a query string. Returns the number applied,
// or -1 if any pair was rejected.
static int applyQuery(const String& query) {
  int applied = 0;
  int pos = 0;

  while (pos < (int)query.length()) {
    int amp = query.indexOf('&', pos);
    if (amp < 0) amp = query.length();

    int eq = query.indexOf('=', pos);
    if (eq < 0 || eq > amp) return -1;

    String key = query.substring(pos, eq);
    String val = query.substring(eq + 1, amp);

    // "ui" is the form's marker for wanting a redirect back, not a setting.
    if (key != "ui") {
      if (!configSet(key.c_str(), val.toInt())) return -1;
      applied++;
    }

    pos = amp + 1;
  }

  return applied;
}

static void redirectHome(WiFiClient& client) {
  client.println(F("HTTP/1.1 303 See Other"));
  client.println(F("Location: /"));
  client.println(F("Connection: close"));
  client.println();
}

// Streams the page rather than building it in a String: the markup lives in
// flash and goes straight out on the socket, so a few KB of HTML costs almost
// no RAM. No Content-Length; the connection close delimits the body.
static void sendPage(WiFiClient& client) {
  const BassConfig& c = config();

  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html; charset=utf-8"));
  client.println(F("Connection: close"));
  client.println();

  client.print(F(
    "<!doctype html><html><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Billy Bass</title><style>"
    ":root{--bg:#0f1115;--fg:#e6e8eb;--mut:#8b93a1;--acc:#4ea1ff;--card:#181b21;--ln:#2a2f39}"
    "*{box-sizing:border-box}"
    "body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 system-ui,-apple-system,sans-serif}"
    ".w{max-width:620px;margin:0 auto;padding:20px}"
    "h1{font-size:19px;margin:0}"
    ".sub{color:var(--mut);font-size:13px;margin:2px 0 18px}"
    ".c{background:var(--card);border-radius:10px;padding:15px;margin-bottom:13px}"
    ".c h2{font-size:11px;text-transform:uppercase;letter-spacing:.09em;color:var(--mut);margin:0 0 11px}"
    ".r{display:flex;justify-content:space-between;padding:3px 0;font-variant-numeric:tabular-nums}"
    ".r span:last-child{color:var(--mut)}"
    ".bar{height:9px;background:var(--ln);border-radius:5px;overflow:hidden;margin:8px 0 4px}"
    ".bar i{display:block;height:100%;background:var(--acc);width:0;transition:width .12s}"
    "label{display:flex;justify-content:space-between;align-items:center;padding:5px 0}"
    "input{background:var(--bg);border:1px solid var(--ln);color:var(--fg);border-radius:6px;padding:7px 9px;font:inherit}"
    "input[type=number]{width:105px}"
    "input[type=text]{width:100%}"
    "button,.b{background:var(--acc);color:#04121f;border:0;border-radius:6px;padding:9px 13px;"
    "font:inherit;font-weight:600;cursor:pointer;text-decoration:none;display:inline-block}"
    ".g{background:var(--ln);color:var(--fg)}"
    ".btns{display:flex;gap:7px;flex-wrap:wrap;margin-top:11px}"
    "a{color:var(--acc)}.f{margin-top:16px;font-size:12px;color:var(--mut)}"
    "</style></head><body><div class=w>"
    "<h1>Billy Bass</h1><div class=sub id=up>&nbsp;</div>"));

  // Live
  client.print(F(
    "<div class=c><h2>Live</h2>"
    "<div class=bar><i id=sb></i></div>"
    "<div class=r><span>sound</span><span id=s>-</span></div>"
    "<div class=r><span>mouth speed</span><span id=m>-</span></div>"
    "<div class=r><span>head</span><span id=h>-</span></div>"
    "<div class=r><span>raw ADC</span><span id=rw>-</span></div>"
    "<div class=r><span>peak-to-peak</span><span id=pp>-</span></div>"
    "<div class=r><span>source</span><span id=sp>-</span></div>"
    "<div class=btns><a class='b g' href=/i2c>Scan I2C bus</a></div></div>"));

  // Say
  client.print(F(
    "<div class=c><h2>Say</h2>"
    "<form action=/speak method=get>"
    "<input type=text name=text maxlength=240 placeholder='type something for the fish to mouth'>"
    "<input type=hidden name=ui value=1>"
    "<div class=btns><button>Speak</button>"
    "<a class='b g' href='/stop?ui=1'>Stop</a></div></form></div>"));

  // Motors
  client.print(F(
    "<div class=c><h2>Motor test</h2>"
    "<div class=btns>"
    "<a class=b href='/motor?m=1&speed=220&ms=1200&ui=1'>M1 mouth</a>"
    "<a class='b g' href='/motor?m=1&speed=220&ms=1200&dir=-1&ui=1'>M1 rev</a>"
    "<a class=b href='/motor?m=2&speed=220&ms=1200&ui=1'>M2 head</a>"
    "<a class='b g' href='/motor?m=2&speed=220&ms=1200&dir=-1&ui=1'>M2 rev</a>"
    "</div></div>"));

  // Config
  client.print(F("<div class=c><h2>Config</h2><form action=/config method=get>"));

  struct Field { const char* key; const char* label; long value; };
  const Field fields[] = {
    { "thresh",  "Sound threshold",     c.soundThreshold },
    { "ppmax",   "Full-scale swing",    c.ppFullScale },
    { "mouthlo", "Mouth speed min",     c.mouthSpeedMin },
    { "mouthhi", "Mouth speed max",     c.mouthSpeedMax },
    { "headspd", "Head speed",          c.headSpeed },
    { "headtmo", "Head timeout (ms)",   (long)c.headTimeoutMs },
    { "spkdly",  "Speak delay (ms)",    (long)c.speakDelayMs },
    { "spkrate", "Speak rate (%)",      c.speakRatePct }
  };

  for (uint8_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
    client.print(F("<label><span>"));
    client.print(fields[i].label);
    client.print(F("</span><input type=number name="));
    client.print(fields[i].key);
    client.print(F(" value="));
    client.print(fields[i].value);
    client.print(F("></label>"));
  }

  client.print(F(
    "<input type=hidden name=ui value=1>"
    "<div class=btns><button>Save</button>"
    "<a class='b g' href='/reset?ui=1'>Defaults</a></div></form></div>"
    "<div class=f>Raw API: <a href=/status>/status</a> &middot; "
    "/speak?text=hi &middot; /config?thresh=40 &middot; /motor?m=1 &middot; "
    "<a href=/i2c>/i2c</a></div>"));

  client.print(F(
    "<script>"
    "function u(){fetch('/status').then(r=>r.text()).then(t=>{var o={};"
    "t.split(/\\s+/).forEach(function(p){var i=p.indexOf('=');if(i>0)o[p.slice(0,i)]=p.slice(i+1)});"
    "var sc=+o.scale||180;"
    "d('s',o.sound);d('m',o.mouth);d('rw',o.raw);d('pp',o.pp);"
    "d('h',o.head=='1'?'active':'idle');"
    "d('sp',o.speaking=='1'?'speech':'microphone');"
    "d('up','uptime '+o.uptime);"
    "document.getElementById('sb').style.width=Math.min(100,(+o.sound)*100/sc)+'%';"
    "}).catch(function(){})}"
    "function d(i,v){var e=document.getElementById(i);if(e)e.textContent=v===undefined?'-':v}"
    "setInterval(u,500);u();"
    "</script></div></body></html>"));
}

static void handle(WiFiClient& client, const String& target) {
  int q = target.indexOf('?');
  String path = (q < 0) ? target : target.substring(0, q);
  String query = (q < 0) ? String() : target.substring(q + 1);

  // Requests from the web UI carry ui=1 and get bounced back to the page;
  // curl and Home Assistant get the plain-text response unchanged.
  bool fromUi = queryValue(query, "ui") == "1";

  if (path == "/" || path == "/index.html") {
    sendPage(client);
    return;
  }

  String body;
  body.reserve(160);

  if (path == "/status") {
    telemetryFormat(body);
    body += '\n';
    configFormat(body);
    body += '\n';
    sendText(client, "200 OK", body);

  } else if (path == "/config") {
    int applied = applyQuery(query);
    if (applied < 0) {
      sendText(client, "400 Bad Request", F("unknown key or value out of range\n"));
      return;
    }
    configSave();
    if (fromUi) { redirectHome(client); return; }
    configFormat(body);
    body += '\n';
    sendText(client, "200 OK", body);

  } else if (path == "/speak") {
    String text = urlDecode(queryValue(query, "text"));
    String delayParam = queryValue(query, "delay");

    unsigned long delayMs = delayParam.length()
      ? (unsigned long)delayParam.toInt()      // per-message override
      : config().speakDelayMs;

    if (!speechSay(text.c_str(), delayMs)) {
      sendText(client, "400 Bad Request", F("text missing or too long\n"));
      return;
    }

    if (fromUi) { redirectHome(client); return; }
    body = "speaking ";
    body += text.length();
    body += " chars after ";
    body += delayMs;
    body += "ms\n";
    sendText(client, "200 OK", body);

  } else if (path == "/stop") {
    speechStop();
    if (fromUi) { redirectHome(client); return; }
    sendText(client, "200 OK", F("stopped\n"));

  } else if (path == "/motor") {
    String mv = queryValue(query, "m");
    String sv = queryValue(query, "speed");
    String dv = queryValue(query, "dir");
    String tv = queryValue(query, "ms");

    bool ok = motorTestRequest(
      (uint8_t)mv.toInt(),
      sv.length() ? (int)sv.toInt() : 220,
      dv.length() ? (int)dv.toInt() : 1,
      tv.length() ? (unsigned long)tv.toInt() : 1200);

    if (!ok) {
      sendText(client, "400 Bad Request", F("use m=1|2, speed=0-255, dir=1|-1, ms>0\n"));
      return;
    }
    if (fromUi) { redirectHome(client); return; }
    sendText(client, "200 OK", F("running\n"));

  } else if (path == "/i2c") {
    // Confirms the shield is actually on the bus. An Adafruit Motor Shield V2
    // answers at its address jumper setting (0x60 by default) and at the
    // 0x70 all-call address.
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire.beginTransmission(addr);
      if (Wire.endTransmission() == 0) {
        body += "0x";
        if (addr < 16) body += '0';
        body += String(addr, HEX);
        body += ' ';
        found++;
      }
    }
    if (found == 0) body = F("no I2C devices responding");
    body += '\n';
    sendText(client, "200 OK", body);

  } else if (path == "/reset") {
    configReset();
    if (fromUi) { redirectHome(client); return; }
    configFormat(body);
    body += '\n';
    sendText(client, "200 OK", body);

  } else {
    sendText(client, "404 Not Found",
             F("try /speak?text=hello+there, /stop, /status, /config?thresh=40, /reset\n"));
  }
}

void httpApiBegin(uint16_t port) {
  if (started) return;

  server.begin(port);
  started = true;
}

void httpApiLoop() {
  if (!started) return;

  WiFiClient client = server.available();
  if (!client) return;

  // Read only the request line; the headers are irrelevant here and draining
  // them costs time we do not want to spend inside loop().
  String line;
  line.reserve(MAX_REQUEST_LINE);
  unsigned long start = millis();

  // Subtraction, not `millis() < start + timeout`, so this still terminates
  // correctly across the millis() rollover.
  while (client.connected() && millis() - start < READ_TIMEOUT_MS) {
    if (!client.available()) continue;

    char c = client.read();
    if (c == '\n') break;
    if (c != '\r' && line.length() < MAX_REQUEST_LINE) line += c;
  }

  // "GET /path?query HTTP/1.1"
  int firstSpace = line.indexOf(' ');
  int secondSpace = line.indexOf(' ', firstSpace + 1);

  if (firstSpace > 0 && secondSpace > firstSpace) {
    handle(client, line.substring(firstSpace + 1, secondSpace));
  } else {
    sendText(client, "400 Bad Request", F("malformed request\n"));
  }

  client.flush();
  client.stop();
}
