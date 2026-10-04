/*
  IBAC V1.0.0 - Icom Bluetooth Amplifier Controller
  Target: Icom IC-705 + HC-05 + Wemos D1 Mini ESP8266 + Xiegu XPA125B

  IMPORTANT HARDWARE NOTES
  ------------------------
  1) Bluetooth carries CI-V frequency/mode data only.
  2) PTT is detected from the IC-705 [SEND/ALC] SEND output (active LOW).
     Do NOT use the IC-705 TUNER socket for this PTT-detect connection.
  3) The XPA125B PTT input is pulled LOW through a 2N2222A open-collector switch.
     Never drive XPA125B ACC pin 2 directly from an ESP8266 GPIO.
  4) XPA125B band voltage is produced by PWM on D2 through a 10k / 10uF RC filter.

  ESP8266 Core: tested by design for 3.1.x API
  Arduino IDE: 2.3.x
  External libraries: none beyond ESP8266 board package.
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <SoftwareSerial.h>

// -----------------------------------------------------------------------------
// Firmware identity
// -----------------------------------------------------------------------------
static const char *FW_NAME = "IBAC";
static const char *FW_VERSION = "1.1.0";

// -----------------------------------------------------------------------------
// Hardware pins (Wemos D1 Mini)
// -----------------------------------------------------------------------------
// SoftwareSerial constructor is (RX pin, TX pin)
static const uint8_t PIN_BT_RX       = D5; // HC-05 TXD -> Wemos D5
static const uint8_t PIN_BT_TX       = D6; // Wemos D6 -> level shifter -> HC-05 RXD
static const uint8_t PIN_PTT_OUT     = D1; // -> 2k -> 2N2222A base
static const uint8_t PIN_BAND_PWM    = D2; // -> 1k -> ACC pin 3, with 10uF to GND
static const uint8_t PIN_RADIO_SEND  = D7; // IC-705 SEND/ALC SEND -> D7, active LOW

// -----------------------------------------------------------------------------
// Timing and safety
// -----------------------------------------------------------------------------
static const unsigned long CIV_FREQ_POLL_MS      = 350;
static const unsigned long CIV_MODE_POLL_MS      = 1500;
static const unsigned long RADIO_STALE_MS        = 3000;
static const unsigned long WIFI_CONNECT_TIMEOUT  = 12000;
static const unsigned long PTT_RELEASE_DELAY_MS  = 15;   // amp stays keyed briefly after radio SEND rises

// -----------------------------------------------------------------------------
// CI-V defaults for IC-705
// -----------------------------------------------------------------------------
static const uint8_t DEFAULT_RADIO_ADDR = 0xA4;
static const uint8_t CONTROLLER_ADDR    = 0xE0;

// -----------------------------------------------------------------------------
// PWM configuration
// -----------------------------------------------------------------------------
static const uint16_t PWM_RANGE = 1023;
static const uint16_t PWM_FREQ_HZ = 5000;

SoftwareSerial btSerial(PIN_BT_RX, PIN_BT_TX);
ESP8266WebServer server(80);

// -----------------------------------------------------------------------------
// Persistent configuration
// -----------------------------------------------------------------------------
struct IBACConfig {
  String wifiSsid;
  String wifiPass;
  uint8_t radioAddress;
  uint16_t pwmFullScaleMv;
  uint16_t bandMv[11];
};

IBACConfig cfg;

// Ordered: 160,80,60,40,30,20,17,15,12,10,6
static const char *BAND_NAMES[11] = {
  "160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"
};

static const uint16_t DEFAULT_BAND_MV[11] = {
  230, 460, 690, 920, 1150, 1380, 1610, 1840, 2070, 2300, 2530
};

// -----------------------------------------------------------------------------
// Runtime radio/amplifier state
// -----------------------------------------------------------------------------
uint64_t radioHz = 0;
String radioMode = "Unknown";
int currentBandIndex = -1;
int appliedBandIndex = -1;
bool radioTx = false;
bool amplifierKeyed = false;
unsigned long lastRadioDataMs = 0;
unsigned long lastFreqPollMs = 0;
unsigned long lastModePollMs = 0;
unsigned long pttReleasedAtMs = 0;
unsigned long civFrames = 0;
unsigned long civBadFrames = 0;
unsigned long bootMs = 0;
String lastCivFrame = "";
String warningText = "Waiting for IC-705 CI-V data";

// CI-V receive parser
uint8_t civBuf[48];
size_t civLen = 0;
bool sawFirstFE = false;
bool inFrame = false;

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
String htmlEscape(const String &s) {
  String r;
  r.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c == '&') r += F("&amp;");
    else if (c == '<') r += F("&lt;");
    else if (c == '>') r += F("&gt;");
    else if (c == '"') r += F("&quot;");
    else r += c;
  }
  return r;
}

String hex2(uint8_t v) {
  char b[3];
  snprintf(b, sizeof(b), "%02X", v);
  return String(b);
}

String formatHz(uint64_t hz) {
  if (!hz) return "--";
  char b[32];
  unsigned long whole = (unsigned long)(hz / 1000000ULL);
  unsigned long frac = (unsigned long)(hz % 1000000ULL);
  snprintf(b, sizeof(b), "%lu.%06lu MHz", whole, frac);
  return String(b);
}

bool radioDataFresh() {
  return lastRadioDataMs != 0 && (millis() - lastRadioDataMs) < RADIO_STALE_MS;
}

bool validRadioAddressText(const String &s, uint8_t &out) {
  if (s.length() < 1 || s.length() > 2) return false;
  char *endp = nullptr;
  long v = strtol(s.c_str(), &endp, 16);
  if (*endp != '\0' || v < 0 || v > 255) return false;
  out = (uint8_t)v;
  return true;
}

// -----------------------------------------------------------------------------
// Configuration persistence - simple key=value text file, no JSON dependency
// -----------------------------------------------------------------------------
void setDefaults() {
  cfg.wifiSsid = "";
  cfg.wifiPass = "";
  cfg.radioAddress = DEFAULT_RADIO_ADDR;
  cfg.pwmFullScaleMv = 3300;
  for (int i = 0; i < 11; ++i) cfg.bandMv[i] = DEFAULT_BAND_MV[i];
}

void saveConfig() {
  File f = LittleFS.open("/ibac.cfg", "w");
  if (!f) return;
  f.println("ssid=" + cfg.wifiSsid);
  f.println("pass=" + cfg.wifiPass);
  f.println("radio=" + hex2(cfg.radioAddress));
  f.println("fullscale=" + String(cfg.pwmFullScaleMv));
  for (int i = 0; i < 11; ++i) {
    f.println("band" + String(i) + "=" + String(cfg.bandMv[i]));
  }
  f.close();
}

void loadConfig() {
  setDefaults();
  if (!LittleFS.exists("/ibac.cfg")) return;
  File f = LittleFS.open("/ibac.cfg", "r");
  if (!f) return;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (!line.length() || line[0] == '#') continue;
    int p = line.indexOf('=');
    if (p < 1) continue;
    String key = line.substring(0, p);
    String val = line.substring(p + 1);
    if (key == "ssid") cfg.wifiSsid = val;
    else if (key == "pass") cfg.wifiPass = val;
    else if (key == "radio") {
      uint8_t a;
      if (validRadioAddressText(val, a)) cfg.radioAddress = a;
    } else if (key == "fullscale") {
      int n = val.toInt();
      if (n >= 2500 && n <= 3600) cfg.pwmFullScaleMv = (uint16_t)n;
    } else if (key.startsWith("band")) {
      int idx = key.substring(4).toInt();
      int n = val.toInt();
      if (idx >= 0 && idx < 11 && n >= 0 && n <= 3300) cfg.bandMv[idx] = (uint16_t)n;
    }
  }
  f.close();
}

// -----------------------------------------------------------------------------
// XPA125B band logic
// -----------------------------------------------------------------------------
int bandForFrequency(uint64_t hz) {
  // Deliberately return -1 for 2m/70cm and for frequencies outside XPA125B HF/6m coverage.
  if (hz >= 1800000ULL  && hz <= 2000000ULL)  return 0;  // 160m
  if (hz >= 3500000ULL  && hz <= 4000000ULL)  return 1;  // 80m
  if (hz >= 5000000ULL  && hz <= 5500000ULL)  return 2;  // 60m
  if (hz >= 7000000ULL  && hz <= 7300000ULL)  return 3;  // 40m
  if (hz >= 10000000ULL && hz <= 10150000ULL) return 4;  // 30m
  if (hz >= 14000000ULL && hz <= 14350000ULL) return 5;  // 20m
  if (hz >= 18068000ULL && hz <= 18168000ULL) return 6;  // 17m
  if (hz >= 21000000ULL && hz <= 21450000ULL) return 7;  // 15m
  if (hz >= 24890000ULL && hz <= 24990000ULL) return 8;  // 12m
  if (hz >= 28000000ULL && hz <= 29700000ULL) return 9;  // 10m
  if (hz >= 50000000ULL && hz <= 54000000ULL) return 10; // 6m
  return -1;
}

uint16_t pwmForMillivolts(uint16_t mv) {
  if (cfg.pwmFullScaleMv < 100) return 0;
  uint32_t p = ((uint32_t)mv * PWM_RANGE + cfg.pwmFullScaleMv / 2) / cfg.pwmFullScaleMv;
  if (p > PWM_RANGE) p = PWM_RANGE;
  return (uint16_t)p;
}

void outputBandVoltage(int bandIndex) {
  if (bandIndex < 0 || bandIndex >= 11) {
    analogWrite(PIN_BAND_PWM, 0);
    appliedBandIndex = -1;
    return;
  }
  analogWrite(PIN_BAND_PWM, pwmForMillivolts(cfg.bandMv[bandIndex]));
  appliedBandIndex = bandIndex;
}

void setAmplifierPtt(bool tx) {
  // 2N2222A open collector: GPIO HIGH = transistor ON = XPA PTT pulled LOW.
  digitalWrite(PIN_PTT_OUT, tx ? HIGH : LOW);
  amplifierKeyed = tx;
}

void safeAmplifierOff() {
  setAmplifierPtt(false);
  outputBandVoltage(-1);
}

void updateAmplifierControl() {
  bool fresh = radioDataFresh();
  bool sendLow = (digitalRead(PIN_RADIO_SEND) == LOW);

  // Track radio PTT using the physical SEND output, never CI-V polling.
  if (sendLow && !radioTx) {
    radioTx = true;
    pttReleasedAtMs = 0;
  } else if (!sendLow && radioTx) {
    radioTx = false;
    pttReleasedAtMs = millis();
  }

  if (!fresh) {
    warningText = "CI-V data stale: amplifier inhibited";
    safeAmplifierOff();
    return;
  }

  currentBandIndex = bandForFrequency(radioHz);

  if (currentBandIndex < 0) {
    warningText = "Frequency not supported by XPA125B: PTT inhibited";
    safeAmplifierOff();
    return;
  }

  warningText = "OK";

  // Never change band voltage while transmitting.
  if (!radioTx && !amplifierKeyed) {
    if (appliedBandIndex != currentBandIndex) outputBandVoltage(currentBandIndex);
  }

  if (radioTx) {
    // Only key if the correct band voltage was already applied before TX.
    if (appliedBandIndex == currentBandIndex) {
      setAmplifierPtt(true);
    } else {
      setAmplifierPtt(false);
      warningText = "PTT inhibited: band was not established before TX";
    }
  } else if (amplifierKeyed) {
    // Hold PTT briefly after IC-705 SEND rises, then release.
    if (pttReleasedAtMs && (millis() - pttReleasedAtMs >= PTT_RELEASE_DELAY_MS)) {
      setAmplifierPtt(false);
      pttReleasedAtMs = 0;
      if (appliedBandIndex != currentBandIndex) outputBandVoltage(currentBandIndex);
    }
  }
}

// -----------------------------------------------------------------------------
// CI-V send / receive
// -----------------------------------------------------------------------------
void civSendCommand(uint8_t cmd) {
  const uint8_t f[] = {0xFE, 0xFE, cfg.radioAddress, CONTROLLER_ADDR, cmd, 0xFD};
  btSerial.write(f, sizeof(f));
}

String frameToHex(const uint8_t *data, size_t len) {
  String s;
  s.reserve(len * 3);
  for (size_t i = 0; i < len; ++i) {
    if (i) s += ' ';
    s += hex2(data[i]);
  }
  return s;
}

uint64_t decodeBcdFrequency(const uint8_t *p, size_t count) {
  // Icom frequency data: decimal digits packed BCD, least significant byte first.
  uint64_t hz = 0;
  uint64_t place = 1;
  for (size_t i = 0; i < count; ++i) {
    uint8_t lo = p[i] & 0x0F;
    uint8_t hi = (p[i] >> 4) & 0x0F;
    if (lo > 9 || hi > 9) return 0;
    hz += (uint64_t)lo * place;
    place *= 10ULL;
    hz += (uint64_t)hi * place;
    place *= 10ULL;
  }
  return hz;
}

String decodeMode(uint8_t m) {
  switch (m) {
    case 0x00: return "LSB";
    case 0x01: return "USB";
    case 0x02: return "AM";
    case 0x03: return "CW";
    case 0x04: return "RTTY";
    case 0x05: return "FM";
    case 0x07: return "CW-R";
    case 0x08: return "RTTY-R";
    case 0x17: return "DV";
    default: return "Mode 0x" + hex2(m);
  }
}

void processCivFrame(const uint8_t *f, size_t len) {
  if (len < 6 || f[0] != 0xFE || f[1] != 0xFE || f[len - 1] != 0xFD) {
    civBadFrames++;
    return;
  }

  civFrames++;
  lastCivFrame = frameToHex(f, len);

  uint8_t dest = f[2];
  uint8_t src  = f[3];
  uint8_t cmd  = f[4];

  // Accept radio responses addressed to us and radio transceive broadcasts.
  bool fromRadio = (src == cfg.radioAddress);
  bool usefulDest = (dest == CONTROLLER_ADDR || dest == 0x00 || dest == 0xE0);
  if (!fromRadio || !usefulDest) return;

  if ((cmd == 0x00 || cmd == 0x03) && len >= 11) {
    // 5 frequency bytes at f[5..9]
    uint64_t hz = decodeBcdFrequency(&f[5], 5);
    if (hz >= 100000ULL && hz <= 500000000ULL) {
      radioHz = hz;
      currentBandIndex = bandForFrequency(radioHz);
      lastRadioDataMs = millis();
    }
  } else if (cmd == 0x04 && len >= 7) {
    radioMode = decodeMode(f[5]);
    lastRadioDataMs = millis();
  }
}

void feedCivByte(uint8_t b) {
  if (!inFrame) {
    if (!sawFirstFE) {
      sawFirstFE = (b == 0xFE);
      return;
    }
    if (b == 0xFE) {
      inFrame = true;
      civLen = 0;
      civBuf[civLen++] = 0xFE;
      civBuf[civLen++] = 0xFE;
    }
    sawFirstFE = (b == 0xFE);
    return;
  }

  if (civLen >= sizeof(civBuf)) {
    civBadFrames++;
    inFrame = false;
    sawFirstFE = false;
    civLen = 0;
    return;
  }

  civBuf[civLen++] = b;
  if (b == 0xFD) {
    processCivFrame(civBuf, civLen);
    inFrame = false;
    sawFirstFE = false;
    civLen = 0;
  }
}

void serviceBluetoothCiv() {
  while (btSerial.available()) {
    feedCivByte((uint8_t)btSerial.read());
    yield();
  }

  unsigned long now = millis();
  if (now - lastFreqPollMs >= CIV_FREQ_POLL_MS) {
    lastFreqPollMs = now;
    civSendCommand(0x03); // read operating frequency
  }
  if (now - lastModePollMs >= CIV_MODE_POLL_MS) {
    lastModePollMs = now;
    civSendCommand(0x04); // read operating mode
  }
}

// -----------------------------------------------------------------------------
// Wi-Fi and web UI
// -----------------------------------------------------------------------------
String apName() {
  String mac = WiFi.softAPmacAddress();
  mac.replace(":", "");
  return "IBAC-" + mac.substring(mac.length() - 4);
}

void startWifi() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apName().c_str(), "IBAC7051");

  if (cfg.wifiSsid.length()) {
    WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());
    unsigned long started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < WIFI_CONNECT_TIMEOUT) {
      delay(100);
    }
  }
}

String navHtml() {
  return F("<nav><a href='/'>Status</a><a href='/settings'>Settings</a><a href='/engineer'>Engineer</a></nav>");
}

String pageHeader(const String &title) {
  String s;
  s.reserve(1800);
  s += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  s += "<title>" + title + F("</title><style>");
  s += F("body{font-family:Arial,sans-serif;max-width:900px;margin:24px auto;padding:0 14px;background:#f4f6f8;color:#1d2733}"
         "h1{margin-bottom:6px}.card{background:white;border-radius:10px;padding:16px;margin:12px 0;box-shadow:0 1px 4px #bbb}"
         "nav a{margin-right:16px}table{border-collapse:collapse;width:100%}td,th{padding:7px;border-bottom:1px solid #ddd;text-align:left}"
         "input{padding:7px;max-width:260px}button{padding:9px 15px}.ok{color:#087830}.bad{color:#b00020;font-weight:bold}code{word-break:break-all}");
  s += F("</style></head><body>");
  s += navHtml();
  s += "<h1>" + title + "</h1>";
  return s;
}

void handleRoot() {
  String s = pageHeader("IBAC Controller");
  s += F("<div class='card'><div id='status'>Loading...</div></div>");
  s += F("<div class='card'><b>Access:</b><br>Setup AP: <code>");
  s += apName();
  s += F("</code> / password <code>IBAC7051</code><br>AP address: <code>192.168.4.1</code>");
  if (WiFi.status() == WL_CONNECTED) {
    s += F("<br>LAN address: <code>"); s += WiFi.localIP().toString(); s += F("</code>");
  }
  s += F("</div><script>async function u(){let r=await fetch('/api/status');let j=await r.json();"
         "document.getElementById('status').innerHTML=`<table>"
         "<tr><th>Firmware</th><td>${j.firmware}</td></tr>"
         "<tr><th>CI-V</th><td>${j.civ}</td></tr>"
         "<tr><th>Frequency</th><td>${j.frequency}</td></tr>"
         "<tr><th>Mode</th><td>${j.mode}</td></tr>"
         "<tr><th>Band</th><td>${j.band}</td></tr>"
         "<tr><th>Radio PTT/SEND</th><td>${j.radio_ptt}</td></tr>"
         "<tr><th>Amplifier PTT</th><td>${j.amp_ptt}</td></tr>"
         "<tr><th>Band target</th><td>${j.band_mv}</td></tr>"
         "<tr><th>Status</th><td>${j.warning}</td></tr></table>`;}u();setInterval(u,1000);</script>");
  s += F("</body></html>");
  server.send(200, "text/html", s);
}

void handleApiStatus() {
  String s = "{";
  s += "\"firmware\":\"" + String(FW_NAME) + " " + String(FW_VERSION) + "\",";
  s += "\"civ\":\"" + String(radioDataFresh() ? "ACTIVE" : "WAITING") + "\",";
  s += "\"frequency\":\"" + formatHz(radioHz) + "\",";
  s += "\"mode\":\"" + radioMode + "\",";
  s += "\"band\":\"" + String(currentBandIndex >= 0 ? BAND_NAMES[currentBandIndex] : "Unsupported") + "\",";
  s += "\"radio_ptt\":\"" + String(radioTx ? "TX" : "RX") + "\",";
  s += "\"amp_ptt\":\"" + String(amplifierKeyed ? "KEYED" : "OFF") + "\",";
  s += "\"band_mv\":\"" + String(currentBandIndex >= 0 ? String(cfg.bandMv[currentBandIndex]) + " mV" : "0 mV") + "\",";
  s += "\"warning\":\"" + warningText + "\"";
  s += "}";
  server.send(200, "application/json", s);
}

void handleSettings() {
  if (server.method() == HTTP_POST) {
    cfg.wifiSsid = server.arg("ssid");
    cfg.wifiPass = server.arg("pass");

    uint8_t a;
    if (validRadioAddressText(server.arg("radio"), a)) cfg.radioAddress = a;

    int fs = server.arg("fullscale").toInt();
    if (fs >= 2500 && fs <= 3600) cfg.pwmFullScaleMv = (uint16_t)fs;

    for (int i = 0; i < 11; ++i) {
      int v = server.arg("b" + String(i)).toInt();
      if (v >= 0 && v <= 3300) cfg.bandMv[i] = (uint16_t)v;
    }
    saveConfig();
    server.sendHeader("Location", "/settings?saved=1");
    server.send(303);
    return;
  }

  String s = pageHeader("IBAC Settings");
  if (server.hasArg("saved")) s += F("<div class='card ok'>Saved. Restart the controller if Wi-Fi settings changed.</div>");
  s += F("<form method='post'><div class='card'><h3>Network</h3><table>");
  s += "<tr><td>Wi-Fi SSID</td><td><input name='ssid' value='" + htmlEscape(cfg.wifiSsid) + "'></td></tr>";
  s += "<tr><td>Wi-Fi password</td><td><input type='password' name='pass' value='" + htmlEscape(cfg.wifiPass) + "'></td></tr>";
  s += F("</table><p>The setup access point remains available even without home Wi-Fi.</p></div>");

  s += F("<div class='card'><h3>Radio / output</h3><table>");
  s += "<tr><td>IC-705 CI-V address (hex)</td><td><input name='radio' value='" + hex2(cfg.radioAddress) + "'></td></tr>";
  s += "<tr><td>Measured PWM full-scale voltage (mV)</td><td><input name='fullscale' type='number' value='" + String(cfg.pwmFullScaleMv) + "'></td></tr>";
  s += F("</table><p>Default IC-705 address is A4. Full-scale is normally near 3300 mV; calibrate it with a multimeter at the RC-filter output if needed.</p></div>");

  s += F("<div class='card'><h3>XPA125B band voltages</h3><table><tr><th>Band</th><th>Target mV</th></tr>");
  for (int i = 0; i < 11; ++i) {
    s += "<tr><td>" + String(BAND_NAMES[i]) + "</td><td><input name='b" + String(i) + "' type='number' value='" + String(cfg.bandMv[i]) + "'></td></tr>";
  }
  s += F("</table><p><button type='submit'>Save settings</button></p></div></form></body></html>");
  server.send(200, "text/html", s);
}

void handleEngineer() {
  String s = pageHeader("IBAC Engineer");
  s += F("<div class='card'><table>");
  s += "<tr><th>Uptime</th><td>" + String((millis() - bootMs) / 1000UL) + " s</td></tr>";
  s += "<tr><th>Free heap</th><td>" + String(ESP.getFreeHeap()) + " bytes</td></tr>";
  s += "<tr><th>CI-V frames</th><td>" + String(civFrames) + "</td></tr>";
  s += "<tr><th>Bad/overflow frames</th><td>" + String(civBadFrames) + "</td></tr>";
  s += "<tr><th>Last CI-V frame</th><td><code>" + lastCivFrame + "</code></td></tr>";
  s += "<tr><th>PWM applied band</th><td>" + String(appliedBandIndex >= 0 ? BAND_NAMES[appliedBandIndex] : "OFF") + "</td></tr>";
  s += "<tr><th>Wi-Fi STA</th><td>" + String(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "Not connected") + "</td></tr>";
  s += F("</table></div><div class='card'><h3>Safety</h3><p>PTT is inhibited if CI-V data is stale, frequency is outside HF/6m, or the correct band was not established before TX.</p></div></body></html>");
  server.send(200, "text/html", s);
}

void startWeb() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/settings", HTTP_POST, handleSettings);
  server.on("/engineer", HTTP_GET, handleEngineer);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
}

// -----------------------------------------------------------------------------
// Arduino setup / loop
// -----------------------------------------------------------------------------
void setup() {
  bootMs = millis();

  pinMode(PIN_PTT_OUT, OUTPUT);
  digitalWrite(PIN_PTT_OUT, LOW);
  pinMode(PIN_BAND_PWM, OUTPUT);
  analogWriteRange(PWM_RANGE);
  analogWriteFreq(PWM_FREQ_HZ);
  analogWrite(PIN_BAND_PWM, 0);

  // IC-705 SEND output is open-collector/active-low. External 10k pull-up to 3V3 recommended.
  pinMode(PIN_RADIO_SEND, INPUT_PULLUP);

  Serial.begin(115200);
  delay(150);
  Serial.println();
  Serial.println(F("========================================"));
  Serial.print(F("IBAC ")); Serial.println(FW_VERSION);
  Serial.println(F("IC-705 Bluetooth -> XPA125B controller"));
  Serial.println(F("========================================"));

  if (!LittleFS.begin()) {
    Serial.println(F("LittleFS mount failed; using defaults."));
    setDefaults();
  } else {
    loadConfig();
  }

  btSerial.begin(9600);
  Serial.println(F("HC-05 UART: 9600 baud on D5(RX)/D6(TX)"));

  startWifi();
  Serial.print(F("Setup AP: ")); Serial.println(apName());
  Serial.println(F("AP password: IBAC7051"));
  Serial.println(F("Open: http://192.168.4.1"));
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("LAN IP: ")); Serial.println(WiFi.localIP());
    if (MDNS.begin("ibac")) Serial.println(F("mDNS: http://ibac.local"));
  }

  startWeb();
  safeAmplifierOff();
}

void loop() {
  serviceBluetoothCiv();
  updateAmplifierControl();
  server.handleClient();
  if (WiFi.status() == WL_CONNECTED) MDNS.update();
  yield();
}
