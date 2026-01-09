#include <esp_wifi.h>
#include <esp_err.h>

// ===== Stable device list cache =====
static const int MAX_TAGS = 20;
static const unsigned long GRACE_MS = 10000; // 10 seconds

uint8_t tagMacs[MAX_TAGS][6];
unsigned long tagLastSeen[MAX_TAGS];
int tagCount = 0;

unsigned long lastScanMs = 0;

// Functions defined in other tabs
String macToString(const uint8_t* mac);
bool addPeerIfNeeded(const uint8_t mac[6]);

// ===== Ignore admin devices (your laptop/phone) so they don't show as tags =====
// Replace these with your real Wi-Fi MAC addresses.
// Format: {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF}

// Example placeholders — CHANGE THEM
static const uint8_t IGNORE_MAC_1[6] = { 0x2A, 0x97, 0x23, 0x53, 0x28, 0x39 }; // laptop Wi-Fi MAC 2A:97:23:53:28:39

static bool macEquals(const uint8_t a[6], const uint8_t b[6]) {
  for (int i = 0; i < 6; i++) if (a[i] != b[i]) return false;
  return true;
}

static bool isZeroMac(const uint8_t mac[6]) {
  for (int i = 0; i < 6; i++) if (mac[i] != 0) return false;
  return true;
}

static bool isIgnoredMac(const uint8_t mac[6]) {
  if (macEquals(mac, IGNORE_MAC_1)) return true;
  return false;
}

static int findTagIndex(const uint8_t mac[6]) {
  for (int i = 0; i < tagCount; i++) {
    if (macEquals(tagMacs[i], mac)) return i;
  }
  return -1;
}

// ---------- Main scan ----------
void rescanStations() {
  wifi_sta_list_t sta_list;
  memset(&sta_list, 0, sizeof(sta_list));

  esp_err_t r = esp_wifi_ap_get_sta_list(&sta_list);
  if (r != ESP_OK) {
    Serial.printf("ap_get_sta_list failed: %s (%d)\n", esp_err_to_name(r), (int)r);
    return;
  }

  unsigned long now = millis();

  // 1) Update "last seen" for all stations currently connected
  for (int i = 0; i < sta_list.num; i++) {
    const uint8_t* mac = sta_list.sta[i].mac;

    // ✅ Skip your laptop/phone so it doesn't appear as a "tag"
    if (isIgnoredMac(mac)) {
      continue;
    }

    int idx = findTagIndex(mac);
    if (idx >= 0) {
      tagLastSeen[idx] = now;
    } else {
      if (tagCount < MAX_TAGS) {
        memcpy(tagMacs[tagCount], mac, 6);
        tagLastSeen[tagCount] = now;
        tagCount++;

        addPeerIfNeeded(mac);

        Serial.printf("+ New tag: %s\n", macToString(mac).c_str());
      } else {
        Serial.println("⚠️ MAX_TAGS reached, cannot add more tags.");
      }
    }
  }

  // 2) Remove tags not seen for GRACE_MS
  for (int i = 0; i < tagCount; ) {
    if (now - tagLastSeen[i] > GRACE_MS) {
      Serial.printf("- Remove tag (timeout): %s\n", macToString(tagMacs[i]).c_str());

      int last = tagCount - 1;
      if (i != last) {
        memcpy(tagMacs[i], tagMacs[last], 6);
        tagLastSeen[i] = tagLastSeen[last];
      }
      tagCount--;
      continue;
    }
    i++;
  }

  // Debug print:
  Serial.printf("Scan: %d tag(s) tracked (AP says %d connected)\n", tagCount, sta_list.num);
}
