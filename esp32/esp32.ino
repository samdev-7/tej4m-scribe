/* Scribe - ESP32 Side
 *
 * By Sam Liu
 * Last updated Dec 5, 2024
 */

#include <SPI.h>
#include <WiFi.h>
#include <NetworkClient.h>
#include <WiFiAP.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <FastLED.h>
#include "LittleFS.h"

// this allows serial to be "turned off" to improve performance
#define DEBUG true
#if (DEBUG)
  #define DEBUG_SERIAL Serial
#endif

// pin definitions
#define PIN_ARDUINO_READY 17
#define PIN_LED 16

static const int spiClk = 1000000; // clock speed set to 1 MHz

const char *ssid = "Scribe";
const char *password = "password";
const char *hostname = "scribe"; 
IPAddress ip;

AsyncWebServer server(80);
DNSServer dnsServer;
AsyncWebSocket ws("/ws");

String dataForArduino = "";

CRGB leds[1];

SPIClass *vspi;
byte msg[2] = {0b00000000, 0b00000000};

unsigned long nonce = 0;

void setup() {
  // init pins
  pinMode(PIN_ARDUINO_READY, INPUT);

  FastLED.addLeds<NEOPIXEL, PIN_LED>(leds, 1);
  setLED(CRGB::Yellow);

  DEBUG_SERIAL.begin(115200); // init serial
  DEBUG_SERIAL.setTimeout(10);

  // debugging: ports of SPI
  DEBUG_SERIAL.print("MOSI: ");
  DEBUG_SERIAL.println(MOSI);
  DEBUG_SERIAL.print("MISO: ");
  DEBUG_SERIAL.println(MISO);
  DEBUG_SERIAL.print("SCK: ");
  DEBUG_SERIAL.println(SCK);
  DEBUG_SERIAL.print("SS: ");
  DEBUG_SERIAL.println(SS);

  // init SPI
  vspi = new SPIClass(VSPI);
  vspi->begin(SCK, MISO, MOSI, SS);
  pinMode(vspi->pinSS(), OUTPUT);
  digitalWrite(vspi->pinSS(), HIGH);

  // init WiFi AP
  DEBUG_SERIAL.println("Configuring access point");
  WiFi.mode(WIFI_AP);
  WiFi.setHostname(hostname);
  if (!WiFi.softAP(ssid, password)) {
    DEBUG_SERIAL.println("Soft AP Creation Failed");
    setLED(CRGB::Red);
    stop();
  }
  DEBUG_SERIAL.print("AP IP address: ");
  ip = WiFi.softAPIP();
  DEBUG_SERIAL.println(ip);
  
  // init DNS server for captive portal
  DEBUG_SERIAL.println("Configuring DNS server");
  if (!dnsServer.start()) {
    DEBUG_SERIAL.println("DNS server failed to start");
    setLED(CRGB::Orange);
    stop();
  }

  // init LittleFS for file serving
  DEBUG_SERIAL.println("Configuring LittleFS");
  if (!LittleFS.begin(true)) {
    DEBUG_SERIAL.println("LittleFS failed to mount");
    setLED(CRGB::Purple);
    stop();
  }

  // init webserver
  DEBUG_SERIAL.println("Configuring webserver");
  configureServer();
  server.begin();

  // init websocket
  DEBUG_SERIAL.println("Configuring websocket server");
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  DEBUG_SERIAL.println("Startup complete");
  setLED(CRGB::Green);
}

void configureServer() {
  // Main control ui webpage
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  // Promps a captive portal which will redirect to the correct page
  server.on("/portal", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("http://" + ip.toString());
  });
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("/portal");
  });

  // serve static web assets
  server.serveStatic("/", LittleFS, "/");
}

// sends a two byte command to the Arduino and optionally check to make sure it worked
bool rawSendToArduino(SPIClass *spi, byte data[], bool check = false, int timeout = 1000, int runTimeout = 60000) {
  // debugging data. note that leading zeros are not printed
  DEBUG_SERIAL.print("Byte 1: ");
  DEBUG_SERIAL.println(msg[0], BIN);
  DEBUG_SERIAL.print("Byte 2: ");
  DEBUG_SERIAL.println(msg[1], BIN);

  unsigned long startSendTime = millis();

  // send the data through SPI
  vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE0));
  digitalWrite(vspi->pinSS(), LOW);
  vspi->transfer16(msg[0] << 8 | msg[1]);
  digitalWrite(vspi->pinSS(), HIGH);
  vspi->endTransaction();

  if (!check) {
    return true;
  } else {
    while (millis() - startSendTime <= timeout) {
      if (digitalRead(PIN_ARDUINO_READY) == LOW) {
        // Arduino not ready, the command has been successfully recieved
        DEBUG_SERIAL.println("Command successfully recieved by Arduino");
        while(true) {
          if (digitalRead(PIN_ARDUINO_READY) == HIGH) {
            DEBUG_SERIAL.println("Command completed by Arduino");
            return true;
          }
          delay(5);
        }
      }
    }
    DEBUG_SERIAL.println("Command timed out");
    return false;
  }
}

// runs on websocket events. we only really care about messages
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      DEBUG_SERIAL.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      DEBUG_SERIAL.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWsMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

// sets the onboard RGB led on the ESP32
void setLED(CRGB::HTMLColorCode color) {
  leds[0] = color; 
  FastLED.setBrightness(10); // low brightness to not be too blinding
  FastLED.show();
}

// haults the program forever 
void stop() {
  while (1) {
    delay(1000);
  }
}

bool sendToArduino(String str) {
  bool valid = true;
  str.trim();
  if (str.length() != 16) {
    DEBUG_SERIAL.println("Invalid input length");
    valid = false;
  }
  for (int i = 0; i < 8 && valid; i++) {
    char c = str[i];
    if (c == '0') {
      msg[0] = msg[0] << 1;
    } else if (c == '1') {
      msg[0] = (msg[0] << 1) | 1;
    } else {
      DEBUG_SERIAL.println("Invalid character");
      valid = false;
      break;
    }
  }
  for (int i = 8; i < 16 && valid; i++) {
    char c = str[i];
    if (c == '0') {
      msg[1] = msg[1] << 1;
    } else if (c == '1') {
      msg[1] = msg[1] << 1 | 1;
    } else {
      DEBUG_SERIAL.println("Invalid character");
      valid = false;
      break;
    }
  }

  if (valid) {
    return rawSendToArduino(vspi, msg, true);
  }
  return false;
}

// runs when a message is recieved via websocket
void handleWsMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;

    // if the message is "ip"
    if (strcmp((char*)data, "ip") == 0) {
      ws.textAll("IP: " + ip.toString());

    // if the message is "nonce"
    } else if (strcmp((char*)data, "nonce") == 0) {
      ws.textAll("NONCE: " + String(nonce));
    } else if (message.startsWith("d")) {
      dataForArduino = message.substring(1);
    }
  }
}

void loop() {
  ws.cleanupClients(); // remove clients that haven't responded in a while

  if (dataForArduino.length() > 0) {
    if (sendToArduino(dataForArduino)) {
      nonce++;
      ws.textAll("NONCE: " + String(nonce));
    } else {
      ws.textAll("ERORR: " + String(nonce));
    }
    dataForArduino = "";
  }

  delay(5); // reduce load on the processor
}
