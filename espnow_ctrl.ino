// Prototypes from main
extern const int ESPNOW_CHANNEL;
extern "C" const char* esp_err_to_name(esp_err_t code);


String macToString(const uint8_t* mac) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

bool parseMacString(const String& s, uint8_t out[6]) {
  // expects "AA:BB:CC:DD:EE:FF"
  if (s.length() != 17) return false;
  int values[6];
  if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x",
             &values[0], &values[1], &values[2],
             &values[3], &values[4], &values[5]) != 6) {
    return false;
  }
  for (int i = 0; i < 6; i++) out[i] = (uint8_t)values[i];
  return true;
}

bool addPeerIfNeeded(const uint8_t mac[6]) {
  if (esp_now_is_peer_exist(mac)) return true;

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;

  // For AP+STA controller, STA ifidx is usually safest for ESP-NOW
  peer.ifidx = WIFI_IF_STA;

  esp_err_t r = esp_now_add_peer(&peer);
  if (r == ESP_OK || r == ESP_ERR_ESPNOW_EXIST) return true;

  Serial.printf("add_peer %s failed: %s (%d)\n",
                macToString(mac).c_str(), esp_err_to_name(r), (int)r);
  return false;
}

esp_err_t sendToOne(const uint8_t mac[6], uint8_t p) {
  Msg m{0, p};
  esp_err_t r = esp_now_send(mac, (uint8_t*)&m, sizeof(m));
  Serial.printf("Send to %s pattern %d -> %s (%d)\n",
                macToString(mac).c_str(), p, esp_err_to_name(r), (int)r);
  return r;
}
