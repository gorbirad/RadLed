#include <Arduino.h>
// ---------------------------------------------
//  LED TRACK CONTROLLER – wersja testowa ESP8266
// ---------------------------------------------

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FastLED.h>
#include "secrets.h"

#ifndef AP_SSID
#define AP_SSID "RadLed-Track"
#endif

#ifndef AP_PASS
#define AP_PASS "radled1234"
#endif

// ------------------- KONFIGURACJA LED --------------------
#define DATA_PIN 4          // D2 = GPIO4
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS 120

CRGB leds[NUM_LEDS];
int globalBrightness = 120;   // domyślna jasność

// ------------------- MULTI-RUNNER SYSTEM -------------------

struct Runner {
  float tempo;        // min/km
  float pos;          // aktualna pozycja
  int length;         // długość okna
  CRGB color;         // kolor
  bool active;        // czy działa

  // NOWE POLA
  float speed;        // LED/s
  float timeToLap;    // sekundy do końca okrążenia
  String hex;         // kolor HEX
  String uuid;        // unikalny identyfikator
  String name;        // nazwa runnera
  String mode;        // tryb animacji
};

#define MAX_RUNNERS 10
Runner runners[MAX_RUNNERS];

// ------------------- PARAMETRY ANIMACJI -------------------
uint32_t lastUpdate = 0;
bool animationRunning = false;
bool reverseDirection = false;

uint32_t startTime = 0;
String lastError = "none";
String wifiMode = "unknown";
IPAddress apIp;

// ------------------- SERWER WWW ---------------------------
ESP8266WebServer server(80);

// ------------------- FUNKCJE ------------------------------

float tempoToLedsPerSecond(float tempo_minpk) {
  float s_per_km = tempo_minpk * 60.0;
  float v_m_s = 1000.0 / s_per_km;
  float leds_per_s = v_m_s * 60.0;  // 60 LED/m
  return leds_per_s;
}

void drawAllRunners() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);

  for (int r = 0; r < MAX_RUNNERS; r++) {
    if (!runners[r].active) continue;

    int start = floor(runners[r].pos);
    int len = runners[r].length;
    if (len <= 0) continue;

    CRGB baseColor = runners[r].color;

    int fadeSize = len / 3;
    if (fadeSize < 1) fadeSize = 1;

    for (int i = 0; i < len; i++) {
      int idx;

      if (!reverseDirection) {
        idx = (start + i) % NUM_LEDS;
      } else {
        idx = (start - i + NUM_LEDS) % NUM_LEDS;
      }

      float brightness = 1.0;

      if (i < fadeSize) {
        brightness = (float)i / fadeSize;
      }
      if (i > len - fadeSize) {
        brightness = (float)(len - i) / fadeSize;
      }

      uint8_t scaled = max(30, (int)(brightness * 255)); // minimalna jasność
      CRGB c = baseColor;
      c.nscale8(scaled);

      leds[idx] = c;
    }
  }
}

// ------------------- Funkcja animacji startowej -------------------------

void startupAnimation() {
  // 1. Fade-in całej taśmy
  for (int b = 0; b <= 255; b += 5) {
    fill_solid(leds, NUM_LEDS, CRGB(b, 0, 0));  // czerwony fade-in
    FastLED.show();
    delay(10);
  }

  // 2. Trzymanie koloru
  delay(500);

  // 3. Przejście czerwony → zielony
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Green;
    FastLED.show();
    delay(5);
  }

  delay(500);

  // 4. Przejście zielony → niebieski
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Blue;
    FastLED.show();
    delay(5);
  }

  delay(500);

  // 5. Fade-out
  for (int b = 255; b >= 0; b -= 5) {
    fill_solid(leds, NUM_LEDS, CRGB(0, 0, b));
    FastLED.show();
    delay(100);
  }

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

void startFlashAnimation() {
  CRGB mixColor = CRGB::Black;
  int count = 0;

  for (int i = 0; i < MAX_RUNNERS; i++) {
    if (runners[i].active) {
      mixColor += runners[i].color;
      count++;
    }
  }

  if (count > 0) {
    mixColor /= count;  // średnia kolorów
  } else {
    mixColor = CRGB::White; // fallback
  }

  fill_solid(leds, NUM_LEDS, mixColor);
  FastLED.show();
  delay(150);

  mixColor.nscale8(80);
  fill_solid(leds, NUM_LEDS, mixColor);
  FastLED.show();
  delay(150);

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

// ------------------- FUNKCJE POMOCNICZE (UUID + HEX) -------------------------

String generateUUID() {
  String uuid = "";
  for (int i = 0; i < 8; i++) {
    uuid += String(random(0, 16), HEX);
  }
  uuid.toUpperCase();
  return uuid;
}

String colorToHex(CRGB c) {
  char buf[8];
  sprintf(buf, "#%02X%02X%02X", c.r, c.g, c.b);
  return String(buf);
}

void connectWiFiWithAPFallback() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  apIp = WiFi.softAPIP();

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Laczenie STA");
  for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    wifiMode = "STA+AP";
    Serial.println("Tryb WiFi: STA+AP");
    Serial.print("STA IP: ");
    Serial.println(WiFi.localIP());
  } else {
    wifiMode = "AP_ONLY";
    Serial.println("Tryb WiFi: AP_ONLY");
    Serial.println("Brak polaczenia STA, dziala tylko Access Point.");
  }

  Serial.print("AP SSID: ");
  Serial.println(AP_SSID);
  Serial.print("AP IP: ");
  Serial.println(apIp);
}

// ------------------- HANDLERY WWW -------------------------

void handleRoot() {
  server.send(200, "text/html",
    "<h2>LED Track Controller</h2>"
    "<p>/addRunner?tempo=5&len=10&r=255&g=0&b=0</p>"
    "<p>/start</p>"
    "<p>/stopAll</p>"
    "<p>/clear</p>"
    "<p>/reverse</p>"
    "<p>/listRunners</p>"
    "<p>/status</p>"
    "<p>/wifiInfo</p>"
  );
}

void handleWifiInfo() {
  String s = "{";
  s += "\"mode\":\"" + wifiMode + "\"";
  s += ",\"apSsid\":\"" + String(AP_SSID) + "\"";
  s += ",\"apIp\":\"" + apIp.toString() + "\"";
  s += ",\"staConnected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
  s += ",\"staIp\":\"" + WiFi.localIP().toString() + "\"";
  s += "}";
  server.send(200, "application/json", s);
}

// ***---- handleStart ----***
void handleStart() {
  animationRunning = true;
  startTime = millis();
  startFlashAnimation();
  server.send(200, "text/plain", "Animation started");
}

// ***---- handleAddRunner ----***
void handleAddRunner() {
  for (int i = 0; i < MAX_RUNNERS; i++) {
    if (!runners[i].active) {

      runners[i].tempo = server.arg("tempo").toFloat();
      runners[i].length = server.arg("len").toInt();
      runners[i].color = CRGB(
        server.arg("r").toInt(),
        server.arg("g").toInt(),
        server.arg("b").toInt()
      );

      runners[i].pos = 0;
      runners[i].active = true;

      // NOWE
      runners[i].speed = tempoToLedsPerSecond(runners[i].tempo);
      runners[i].timeToLap = NUM_LEDS / runners[i].speed;
      runners[i].hex = colorToHex(runners[i].color);
      runners[i].uuid = generateUUID();
      runners[i].name = "Runner " + String(i);
      runners[i].mode = "runner";

      server.send(200, "text/plain", "Runner added at slot " + String(i));
      return;
    }
  }

  server.send(400, "text/plain", "No free runner slots");
}

// ***---- handleStopAll ----***
void handleStopAll() {
  animationRunning = false;   // zatrzymujemy animację, ale NIE ruszamy runnerów

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  server.send(200, "text/plain", "Animation stopped (runners preserved)");
}

// ***---- handleStopRunner ----***
void handleStopRunner() {
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "Missing id");
    return;
  }

  int id = server.arg("id").toInt();

  if (id < 0 || id >= MAX_RUNNERS) {
    server.send(400, "text/plain", "Invalid id");
    return;
  }

  if (!runners[id].active) {
    server.send(200, "text/plain", "Runner already stopped: " + String(id));
    return;
  }

  runners[id].active = false;
  runners[id].pos = 0;
  runners[id].tempo = 0;
  runners[id].length = 0;
  runners[id].speed = 0;
  runners[id].timeToLap = 0;
  runners[id].color = CRGB::Black;
  runners[id].hex = "#000000";
  runners[id].mode = "stopped";

  // Natychmiastowe odrysowanie paska bez zatrzymanego runnera.
  drawAllRunners();
  FastLED.show();

  server.send(200, "text/plain", "Runner stopped and cleared: " + String(id));
}

// ***---- handleClear ----***
void handleClear() {
  for (int i = 0; i < MAX_RUNNERS; i++) {
    runners[i].active = false;
    runners[i].pos = 0;
    runners[i].tempo = 0;
    runners[i].length = 0;
    runners[i].color = CRGB::Black;
  }

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  startupAnimation();

  server.send(200, "text/plain", "All runners cleared and LEDs reset");
}

// ***---- handleListRunners ----***
void handleListRunners() {
  String json = "[";
  bool first = true;

  for (int i = 0; i < MAX_RUNNERS; i++) {
    if (!runners[i].active) continue;

    if (!first) json += ",";
    first = false;

    json += "{";
    json += "\"id\":" + String(i);
    json += ",\"uuid\":\"" + runners[i].uuid + "\"";
    json += ",\"tempo\":" + String(runners[i].tempo, 2);
    json += ",\"speed\":" + String(runners[i].speed, 2);
    json += ",\"pos\":" + String(runners[i].pos, 2);
    json += ",\"length\":" + String(runners[i].length);
    json += ",\"timeToLap\":" + String(runners[i].timeToLap, 2);
    json += ",\"hex\":\"" + runners[i].hex + "\"";
    json += ",\"name\":\"" + runners[i].name + "\"";
    json += ",\"mode\":\"" + runners[i].mode + "\"";

    json += ",\"color\":{\"r\":" + String(runners[i].color.r);
    json += ",\"g\":" + String(runners[i].color.g);
    json += ",\"b\":" + String(runners[i].color.b) + "}";

    json += "}";
  }

  json += "]";
  server.send(200, "application/json", json);
}

// ***---- handleStatus ----***
void handleStatus() {
  String s = "{";

  s += "\"animationRunning\":" + String(animationRunning ? "true" : "false");
  s += ",\"reverseDirection\":" + String(reverseDirection ? "true" : "false");
  s += ",\"elapsed\":" + String((millis() - startTime) / 1000.0, 2);
  s += ",\"trackLength\":" + String(NUM_LEDS);
  s += ",\"error\":\"" + lastError + "\"";
  s += ",\"brightness\":" + String(globalBrightness);
  s += ",\"wifiMode\":\"" + wifiMode + "\"";

  s += "}";

  server.send(200, "application/json", s);
}

// ***---- handleReverse ----***
void handleReverse() {
  reverseDirection = !reverseDirection;

  String msg = reverseDirection
    ? "Kierunek LED: ODWROCONY"
    : "Kierunek LED: STANDARDOWY";

  server.send(200, "text/plain", msg);
}

// ***---- handleBrightness ----***
void handleBrightness() {
  if (!server.hasArg("value")) {
    server.send(400, "text/plain", "Missing value");
    return;
  }

  globalBrightness = server.arg("value").toInt();
  globalBrightness = constrain(globalBrightness, 0, 255);

  FastLED.setBrightness(globalBrightness);
  FastLED.show();

  server.send(200, "text/plain", "Brightness set to " + String(globalBrightness));
}

// ------------------- SETUP -------------------------------

void setup() {
  Serial.begin(115200);
  delay(200);

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(120); //0-255

  connectWiFiWithAPFallback();

  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/addRunner", handleAddRunner);
  server.on("/status", handleStatus);
  server.on("/stopAll", handleStopAll);
  server.on("/stopall", handleStopAll);
  server.on("/stopRunner", handleStopRunner);
  server.on("/clear", handleClear);
  server.on("/listRunners", handleListRunners);
  server.on("/listrunners", handleListRunners);
  server.on("/reverse", handleReverse);
  server.on("/brightness", handleBrightness);
  server.on("/wifiInfo", handleWifiInfo);
  server.on("/wifiinfo", handleWifiInfo);
  server.begin();

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();

  startupAnimation();

  for (int i = 0; i < MAX_RUNNERS; i++) {
    runners[i].active = false;
  }

  lastUpdate = millis();
}

// ------------------- LOOP -------------------------------

void loop() {
  server.handleClient();

  uint32_t now = millis();
  float dt = (now - lastUpdate) / 1000.0;

  if (dt >= 0.02) {  // 50 FPS
    lastUpdate = now;

    for (int r = 0; r < MAX_RUNNERS; r++) {
      if (!animationRunning || !runners[r].active) continue;

      float speed = tempoToLedsPerSecond(runners[r].tempo);

      if (!reverseDirection) {
        runners[r].pos += speed * dt;
      } else {
        runners[r].pos -= speed * dt;
      }

      if (runners[r].pos >= NUM_LEDS) runners[r].pos -= NUM_LEDS;
      if (runners[r].pos < 0)         runners[r].pos += NUM_LEDS;
    // --- aktualizacja speed i timeToLap ---
    runners[r].speed = speed;
    runners[r].timeToLap = (NUM_LEDS - runners[r].pos) / runners[r].speed;
    }

    drawAllRunners();
    FastLED.show();
  }
}