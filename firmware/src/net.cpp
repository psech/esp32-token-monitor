#include "net.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "secrets.h"

namespace net {

namespace {

constexpr unsigned long CONNECT_TIMEOUT_MS = 10000;

// status() alone can't be trusted here: it is updated from async events, and
// tearing the radio down then light-sleeping leaves the previous session's
// WL_CONNECTED in place. The interface IP is cleared on teardown, so it is reliable.
bool isConnected() {
  return WiFi.status() == WL_CONNECTED && (uint32_t)WiFi.localIP() != 0;
}

bool connect() {
  const unsigned long start = millis();
  // Not persistent: otherwise every begin() rewrites the Wi-Fi config to flash.
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  // Modem power-save buys nothing in a sub-second session and makes the AP
  // buffer our first packets.
  WiFi.setSleep(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (!isConnected()) {
    if (millis() - start > CONNECT_TIMEOUT_MS) {
      Serial.println("[wifi] connect timeout");
      return false;
    }
    delay(20);
  }
  // On this LAN the first packets sent straight after DHCP completes are often
  // dropped, costing a 3 s TCP retransmit; a short pause measured cheaper.
  delay(100);

  Serial.printf("[wifi] connected in %lums, ip=%s rssi=%d\n",
                millis() - start, WiFi.localIP().toString().c_str(), WiFi.RSSI());
  return true;
}

void radioOff() {
  WiFi.disconnect(true);
}

bool get(UsageSnapshot& out) {
  HTTPClient http;
  char url[96];
  snprintf(url, sizeof(url), "http://%s:%u/api/usage", SERVICE_HOST, SERVICE_PORT);

  // A lost SYN otherwise waits out lwIP's 3 s retransmit; failing fast lets
  // the caller's retry go out instead. LAN round-trips are single-digit ms.
  http.setConnectTimeout(1000);
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

} // namespace

bool fetchUsage(UsageSnapshot& out) {
  const unsigned long start = millis();
  // The first request right after association is occasionally lost; one
  // retry while the radio is already up is far cheaper than a failed poll.
  const bool ok = connect() && (get(out) || get(out));
  radioOff();
  Serial.printf("[net] radio on %lums\n", millis() - start);
  return ok;
}

} // namespace net
