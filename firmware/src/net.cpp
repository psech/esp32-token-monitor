#include "net.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "secrets.h"

namespace net {

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[wifi] connecting to %s", WIFI_SSID);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(250);
    if (millis() - start > 30000) {
      Serial.println("\n[wifi] timeout, restarting wifi");
      WiFi.disconnect(true);
      delay(500);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      start = millis();
    }
  }
  Serial.printf("\n[wifi] connected, ip=%s rssi=%d\n",
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

bool isOnline() {
  return WiFi.status() == WL_CONNECTED;
}

bool fetchUsage(UsageSnapshot& out) {
  if (!isOnline()) {
    Serial.println("[net] offline, skipping fetch");
    return false;
  }

  HTTPClient http;
  char url[96];
  snprintf(url, sizeof(url), "http://%s:%u/api/usage", SERVICE_HOST, SERVICE_PORT);

  http.setTimeout(5000);
  if (!http.begin(url)) {
    Serial.println("[net] http.begin failed");
    return false;
  }

  int code = http.GET();
  if (code != 200) {
    Serial.printf("[net] GET %s -> %d\n", url, code);
    http.end();
    return false;
  }

  // Filter to keep only the fields we care about — keeps RAM use tiny.
  JsonDocument filter;
  filter["session"]["percent"] = true;
  filter["week"]["percent"]    = true;
  filter["stale"]              = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream(),
                                             DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("[net] json parse: %s\n", err.c_str());
    return false;
  }

  out.sessionPct = doc["session"]["percent"] | 0;
  out.weekPct    = doc["week"]["percent"]    | 0;
  out.stale      = doc["stale"]              | false;
  return true;
}

} // namespace net
