#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_err.h>

#include "web_ui.h"

// ===== Config =====
static const int ESPNOW_CHANNEL = 1;
const char* AP_SSID = "TAG_REMOTE";
const char* AP_PASS = "12345678";

// Web server (global)
WebServer server(80);

// Message format (same as your tags)
typedef struct __attribute__((packed)) {
  uint8_t cmd;   // 0=setPattern
  uint8_t value; // 0..4
} Msg;

// Shared device list cache (defined in devices.ino)
extern uint8_t tagMacs[][6];
extern int tagCount;
extern unsigned long lastScanMs;

// Prototypes (implemented in other tabs)
String macToString(const uint8_t* mac);
bool parseMacString(const String& s, uint8_t out[6]);
bool addPeerIfNeeded(const uint8_t mac[6]);
esp_err_t sendToOne(const uint8_t mac[6], uint8_t p);
void rescanStations();

void handleRoot();
void handleDevices();
void handleSet();

// ===== Wi-Fi events help us refresh faster =====
volatile bool stationsDirty = false;

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
    stationsDirty = true;
    Serial.printf("[AP] STA connected: %02X:%02X:%02X:%02X:%02X:%02X\n",
      info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1],
      info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
      info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5]);
  }

  if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
    stationsDirty = true;
    Serial.printf("[AP] STA disconnected: %02X:%02X:%02X:%02X:%02X:%02X\n",
      info.wifi_ap_stadisconnected.mac[0], info.wifi_ap_stadisconnected.mac[1],
      info.wifi_ap_stadisconnected.mac[2], info.wifi_ap_stadisconnected.mac[3],
      info.wifi_ap_stadisconnected.mac[4], info.wifi_ap_stadisconnected.mac[5]);
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Controller (Auto-discovery via AP station list) ===");

  // Listen Wi-Fi events (connect/disconnect)
  WiFi.onEvent(onWiFiEvent);

  // AP + STA helps ESP-NOW
  WiFi.mode(WIFI_AP_STA);

  // Start AP on fixed channel
  if (!WiFi.softAP(AP_SSID, AP_PASS, ESPNOW_CHANNEL)) {
    Serial.println("❌ Failed to start AP");
    while (true) delay(1000);
  }

  // ✅ IMPORTANT: kick “dead” stations faster (unplugged tags)
  // This makes esp_wifi_ap_get_sta_list update sooner.
  esp_wifi_set_inactive_time(WIFI_IF_AP, 5); // seconds

  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
  Serial.println("Open: http://192.168.4.1");

  // Force channel
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ esp_now_init failed");
    while (true) delay(1000);
  }
  Serial.println("✅ ESP-NOW ready");

  // Initial scan
  rescanStations();

  // Web routes
  server.on("/", handleRoot);
  server.on("/devices", handleDevices);
  server.on("/set", handleSet);
  server.begin();
  Serial.println("✅ Web server started");
}

void loop() {
  server.handleClient();

  // Refresh scan if event happened, or every 3 seconds
  if (stationsDirty || (millis() - lastScanMs > 3000)) {
    stationsDirty = false;
    lastScanMs = millis();
    rescanStations();
  }
}
