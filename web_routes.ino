extern WebServer server;

// from other tabs
extern int tagCount;
extern uint8_t tagMacs[][6];

String macToString(const uint8_t* mac);
bool parseMacString(const String& s, uint8_t out[6]);
void rescanStations();
bool addPeerIfNeeded(const uint8_t mac[6]);
esp_err_t sendToOne(const uint8_t mac[6], uint8_t p);

extern const char INDEX_HTML[] PROGMEM;

void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

void handleDevices() {
  rescanStations();

  String json = "[";
  for (int i = 0; i < tagCount; i++) {
    if (i) json += ",";
    json += "\"" + macToString(tagMacs[i]) + "\"";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleSet() {
  if (!server.hasArg("c")) {
    server.send(400, "text/plain", "Missing ?c=0..4");
    return;
  }
  int c = server.arg("c").toInt();
  if (c < 0 || c > 4) {
    server.send(400, "text/plain", "c must be 0..4");
    return;
  }

  String t = server.hasArg("t") ? server.arg("t") : "all";

  rescanStations();

  if (t == "all") {
    int sent = 0;
    for (int i = 0; i < tagCount; i++) {
      if (sendToOne(tagMacs[i], (uint8_t)c) == ESP_OK) sent++;
      delay(5);
    }
    server.send(200, "text/plain",
      "Sent pattern " + String(c) + " to ALL tags (" + String(sent) + "/" + String(tagCount) + ")");
    return;
  }

  uint8_t mac[6];
  if (!parseMacString(t, mac)) {
    server.send(400, "text/plain", "Bad target MAC format. Expected AA:BB:CC:DD:EE:FF");
    return;
  }

  addPeerIfNeeded(mac);
  esp_err_t r = sendToOne(mac, (uint8_t)c);
  server.send(200, "text/plain", "Sent pattern " + String(c) + " to " + t + " -> " + String(esp_err_to_name(r)));
}
