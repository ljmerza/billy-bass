#include <WiFiS3.h>
#include <Wire.h>
#include "http_api.h"
#include "config.h"
#include "telemetry.h"
#include "speech.h"
#include "motortest.h"

static const unsigned long READ_TIMEOUT_MS = 500;
// Long enough for the whole config form in one GET. Every field is submitted
// together, so this grows with the number of config keys - at 18 fields the
// request line is already past 200, which is what the old cap silently
// truncated into a "malformed request". At 27 fields with worst-case values it
// is around 400, so this keeps a real margin rather than the last few bytes.
static const size_t MAX_REQUEST_LINE = 768;

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
    "label{display:flex;justify-content:space-between;align-items:center;padding:5px 0;position:relative}"
    ".c h3{font-size:11px;text-transform:uppercase;letter-spacing:.09em;color:var(--acc);margin:16px 0 4px;padding-top:12px;border-top:1px solid var(--ln)}"
    ".c h3:first-of-type{margin-top:2px;padding-top:0;border-top:0}"
    ".q{font-style:normal;display:inline-block;width:16px;height:16px;line-height:16px;text-align:center;border-radius:50%;background:var(--ln);color:var(--mut);font-size:10px;cursor:help;vertical-align:1px}"
    ".q:hover,.q:focus{background:var(--acc);color:#04121f;outline:none}"
    ".q::after{content:attr(data-t);display:none;position:absolute;left:0;right:0;top:100%;background:#05070a;border:1px solid var(--ln);border-radius:7px;padding:8px 10px;color:var(--fg);font:12px/1.45 system-ui,sans-serif;text-align:left;z-index:9;box-shadow:0 6px 18px #0009}"
    ".q:hover::after,.q:focus::after{display:block}"
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
    "<div class=r><span>tail</span><span id=t>-</span></div>"
    "<div class=r><span>raw ADC</span><span id=rw>-</span></div>"
    "<div class=r><span>peak-to-peak (voice)</span><span id=pp>-</span></div>"
    "<div class=r><span>peak-to-peak (raw)</span><span id=ppr>-</span></div>"
    "<div class=r><span>threshold</span><span id=thr>-</span></div>"
    "<div class=r><span>running mean</span><span id=avg>-</span></div>"
    "<div class=r><span>samples/window</span><span id=smp>-</span></div>"
    "<div class=r><span>source</span><span id=sp>-</span></div>"
    "<div class=r><span>pitch</span><span id=f0>-</span></div>"
    "<div class=r><span>periodicity</span><span id=vc>-</span></div>"
    "<div class=r><span>voiced / smooth / range</span><span id=vf>-</span></div>"
    "<div class=r><span>voice score</span><span id=vs>-</span></div>"
    "<div class=r><span>voice gate</span><span id=vo>-</span></div>"
    "<div class=r><span>button</span><span id=bt>-</span></div>"
    "<div class=r><span>button presses</span><span id=btc>-</span></div>"
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
  // Forward only. The mouth, head and tail all drive against a return spring
  // into a hard stop, so there is no reverse test to offer.
  client.print(F(
    "<div class=c><h2>Motor test</h2>"
    "<div class=btns>"
    "<a class=b href='/motor?m=1&speed=220&ms=1200&ui=1'>M1 mouth</a>"
    "<a class=b href='/motor?m=2&speed=220&ms=1200&ui=1'>M2 head</a>"
    "<a class=b href='/motor?m=3&speed=200&ms=1200&ui=1'>M3 tail</a>"
    "</div></div>"));

  // Config. Grouped by what each knob actually moves, because a flat list of
  // fifteen numbers gives no clue which ones interact. Every row carries the
  // explanation and the accepted range as a tooltip - that information used to
  // exist only as comments in config.cpp, where nobody tuning the fish from a
  // phone was ever going to read it.
  client.print(F("<div class=c><h2>Config</h2><form action=/config method=get>"));

  struct Field { const char* key; const char* label; long value; const char* help; };
  struct Section { const char* title; const Field* fields; uint8_t count; };

  const Field general[] = {
    { "thresh",  "Sound threshold", c.soundThreshold,
      "How loud sound has to be before the fish reacts at all, 0-1023 (180 is the loudest "
      "reading). Bigger = quiet sounds are ignored. Smaller = the fish reacts to more, "
      "including room noise. Try 30." },
    { "ppmax",   "Full-scale swing", c.ppFullScale,
      "How loud your audio gets at its loudest, 10-4095. Set this to match reality: watch pp "
      "on the Live panel while your audio plays and set this just above the biggest number "
      "you see. Too small = the mouth slams the same on every word. Too big = the mouth "
      "barely moves." },
    { "voice",   "Voice filter (0/1)", c.voiceFilter,
      "1 = only listen to the frequencies people talk in. 0 = listen to everything, including "
      "bass and hiss. Leave it at 1 - with 0, music holds the mouth open the whole track." },
    { "adapt",   "Adaptive thresh (%)", c.adaptPct,
      "Ignores anything quieter than this percentage of the recent average, 0-300. Bigger = "
      "the mouth skips more words. 0 = off. Use 0. This is only useful with Articulate "
      "turned off." },
    { "spkdly",  "Speak delay (ms)", (long)c.speakDelayMs,
      "Waits this long with the mouth shut before starting a /speak line, 0-5000ms. Use it to "
      "line the mouth up with audio playing on a speaker. Bigger = the fish starts later. "
      "Try 0." },
    { "spkrate", "Speak rate (%)", c.speakRatePct,
      "How fast the mouth moves through a /speak line, 50-200. Bigger = talks faster. "
      "Smaller = talks slower. 100 is normal." }
  };

  const Field head[] = {
    { "headq",   "Quiet drive (0/1)", c.headQuiet,
      "1 = the head motor is only ever fully on or fully off, which stops it putting a buzz "
      "on the speaker. 0 = the motor can run at part power, which is smoother but can hum. "
      "With 1 on, Head speed and Head hold power do nothing - use Hold on and Hold off "
      "instead." },
    { "headspd", "Head speed", c.headSpeed,
      "How hard the motor pushes the head out, 0-255. Bigger = swings out faster and harder. "
      "Smaller = lazier. Does nothing while Quiet drive is 1. Try 254." },
    { "headmv",  "Head travel (ms)", (long)c.headMoveMs,
      "How long the motor pushes to swing the head out, 0-5000ms. Bigger = the head comes "
      "out further, until it hits its stop. Smaller = only part way. Try 400." },
    { "headhld", "Head hold power", c.headHoldSpeed,
      "How hard the motor works to keep the head out once it is there, 0-255. Bigger = holds "
      "firmer but the motor stays under strain. 0 = let it fall straight back in. Does "
      "nothing while Quiet drive is 1. Try 120." },
    { "headhon", "Hold on (ms)", (long)c.headHoldOnMs,
      "How long the head motor pulls during each hold pulse, 0 or 20-1000ms. Bigger = holds "
      "the head out firmer. 0 = do not hold at all, let the head fall back after it swings "
      "out. Only used while Quiet drive is 1. Try 30." },
    { "headhof", "Hold off (ms)", (long)c.headHoldOffMs,
      "How long the head motor rests between those pulls, 20-2000ms. Bigger = the head "
      "droops and bobs more. Smaller = holds steadier. Only used while Quiet drive is 1. "
      "Try 40." },
    { "headtmo", "Stay out (ms)", (long)c.headTimeoutMs,
      "How long the head stays out after the sound stops, 0-600000ms. Bigger = stays out "
      "longer after a noise. Smaller = snaps back quickly. More sound restarts the clock. "
      "Try 3000." }
  };

  const Field tail[] = {
    { "tailsnd", "Tail on sound (0/1)", c.tailSoundDriven,
      "1 = the tail flaps whenever the fish hears something. 0 = the tail only moves during a "
      "/speak line and ignores the microphone completely." },
    { "tailen",  "Tail enabled (0/1)", c.tailEnabled,
      "0 = park the tail and leave the mouth and head working. Use it for a quieter fish, or "
      "to rule the tail motor out while you are tuning something else." },
    { "tailspd", "Tail speed", c.tailSpeed,
      "How hard the tail motor pulls, 0-255. Bigger = a wider, harder flap. Smaller = a "
      "gentler one. Try 200." },
    { "tailms",  "Tail flap (ms)", (long)c.tailFlapMs,
      "How long each half of a flap takes, 40-2000ms - it pulls for this long, then rests for "
      "this long so the spring swings it back. Bigger = slow lazy flaps. Smaller = fast "
      "flaps. Below 40 the tail just buzzes. Try 150." }
  };

  const Field mouth[] = {
    { "mouthlo", "Mouth speed min", c.mouthSpeedMin,
      "How hard the mouth opens on the quietest sound it reacts to, 0-255. Bigger = even "
      "quiet words move the jaw properly. Smaller = quiet words barely twitch. Try 170." },
    { "mouthhi", "Mouth speed max", c.mouthSpeedMax,
      "How hard the mouth opens on the loudest sound, 0-255. Bigger = loud words snap the jaw "
      "wide. Everything between quiet and loud lands between this and Mouth speed min, so a "
      "gap between the two is what makes loud words look different from quiet ones. Try "
      "254." },
    { "artic",   "Articulate (0/1)", c.articulate,
      "1 = the mouth opens and shuts in bursts, one per word, which is what makes it look "
      "like talking. 0 = the mouth is held open the whole time there is sound, so it hangs "
      "open through music. Use 1." },
    { "mouthon", "Mouth pulse on (ms)", (long)c.mouthOnMs,
      "How long the mouth is pulled open in each burst, 20-1000ms. Bigger = wider, slower "
      "mouth movements. Smaller = quick small ones, and under about 20 it only twitches. "
      "Only used while Articulate is 1. Try 70." },
    { "mouthof", "Mouth pulse off (ms)", (long)c.mouthOffMs,
      "How long the mouth is guaranteed shut between bursts, 20-1000ms. Bigger = fewer, more "
      "separated mouth movements. Smaller = faster chattering, but the mouth may not fully "
      "close. Only used while Articulate is 1. Try 50." }
  };

  const Field voice[] = {
    { "vgate",  "Voice gate (0/1)", c.voiceGate,
      "1 = only react to sounds that are actually a person talking, and ignore music, fans "
      "and bangs. 0 = react to any sound loud enough. Set the three settings below first with "
      "this at 0, watching the Voice score on the Live panel while your audio plays, then "
      "turn it on. It takes about half a second of talking before the fish starts moving." },
    { "vconf",  "Periodicity min", c.voiceConfMin,
      "How much a sound has to sound like a voice rather than noise, 0-100. Bigger = noise "
      "stops counting as a voice, but a quiet or far-away person might too. Smaller = picks "
      "up quiet voices, and more junk. Try 55." },
    { "vscore", "Voice score min", c.voiceScoreMin,
      "How sure the fish has to be before it decides it is hearing a voice, 0-100. Bigger = "
      "fewer false starts on music, but real talking gets missed. Smaller = reacts more "
      "easily to anything. Watch Voice score on the Live panel while your audio plays and set "
      "this just under what real talking reaches. Try 55." },
    { "vhold",  "Gate hold (ms)", (long)c.voiceHoldMs,
      "How long the fish keeps going after someone stops talking, 100-30000ms. Bigger = it "
      "rides through the gaps between sentences, but a single spoken word lets a whole song "
      "through behind it. Smaller = it stops dead mid-sentence. Try 1500." }
  };

  const Field button[] = {
    { "btnlong", "Long press (ms)", (long)c.btnLongMs,
      "How long the button has to be held to count as a long press instead of a short one, "
      "200-5000ms. Bigger = you have to hold it longer. The fish itself does nothing with "
      "either press - both are sent to Home Assistant, and what happens next is set up "
      "there. Try 700." }
  };

  const Section sections[] = {
    { "General", general, sizeof(general) / sizeof(general[0]) },
    { "Head",    head,    sizeof(head)    / sizeof(head[0])    },
    { "Tail",    tail,    sizeof(tail)    / sizeof(tail[0])    },
    { "Mouth",   mouth,   sizeof(mouth)   / sizeof(mouth[0])   },
    { "Voice",   voice,   sizeof(voice)   / sizeof(voice[0])   },
    { "Button",  button,  sizeof(button)  / sizeof(button[0])  }
  };

  for (uint8_t s = 0; s < sizeof(sections) / sizeof(sections[0]); s++) {
    client.print(F("<h3>"));
    client.print(sections[s].title);
    client.print(F("</h3>"));

    for (uint8_t i = 0; i < sections[s].count; i++) {
      const Field& f = sections[s].fields[i];
      client.print(F("<label><span>"));
      client.print(f.label);
      client.print(F(" <i class=q tabindex=0 data-t=\""));
      client.print(f.help);
      client.print(F("\">?</i></span><input type=number name="));
      client.print(f.key);
      client.print(F(" value="));
      client.print(f.value);
      client.print(F("></label>"));
    }
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
    "d('ppr',o.ppraw);d('thr',o.thr);d('avg',o.avg);d('smp',o.samples);"
    "d('h',o.head=='1'?'active':'idle');"
    "d('t',o.tail=='1'?'flapping':'idle');"
    "d('sp',o.speaking=='1'?'speech':'microphone');"
    "d('bt',o.btn=='1'?'pressed':'up');"
    "d('btc',o.btnn);"
    "d('f0',o.f0>0?o.f0+' Hz':'none');"
    "d('vc',o.vconf);"
    "d('vf',o.vpct+' / '+o.vsmooth+' / '+o.vrange+'%');"
    "d('vs',o.vscore);"
    "d('vo',o.vopen=='1'?'open':'closed');"
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
    String tv = queryValue(query, "ms");

    // An explicit dir= is rejected rather than ignored. Silently running forward
    // on dir=-1 would look like the reverse test worked and the motor failed.
    if (queryValue(query, "dir").length()) {
      sendText(client, "400 Bad Request",
               F("dir is not supported - the motors are forward-only\n"));
      return;
    }

    bool ok = motorTestRequest(
      (uint8_t)mv.toInt(),
      sv.length() ? (int)sv.toInt() : 220,
      tv.length() ? (unsigned long)tv.toInt() : 1200);

    if (!ok) {
      sendText(client, "400 Bad Request", F("use m=1|2|3, speed=0-255, ms>0\n"));
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
  bool truncated = false;

  while (client.connected() && millis() - start < READ_TIMEOUT_MS) {
    if (!client.available()) continue;

    char c = client.read();
    if (c == '\n') break;
    if (c == '\r') continue;

    if (line.length() < MAX_REQUEST_LINE) line += c;
    else truncated = true;      // keep draining, but remember it did not fit
  }

  // Truncation is reported for what it is. Silently cutting the line loses the
  // trailing " HTTP/1.1", so the parse below finds no second space and the
  // request comes back as "malformed" - which sends you looking at the query
  // string instead of at its length.
  if (truncated) {
    sendText(client, "414 URI Too Long", F("request line over 512 bytes - split the config into two requests\n"));
    client.flush();
    client.stop();
    return;
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
