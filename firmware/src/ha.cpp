#include "ha.h"

#include "secrets.h"

#ifndef MQTT_HOST

namespace ha {
void publish(const BatteryReading&, int, const UsageSnapshot*) {}
} // namespace ha

#else

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <lwip/sockets.h>

#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif
#ifndef HA_DEVICE_NAME
#define HA_DEVICE_NAME "Token Monitor"
#endif
#ifndef HA_DISCOVERY_PREFIX
#define HA_DISCOVERY_PREFIX "homeassistant"
#endif

namespace ha {

namespace {

// The device is only online for a moment each poll, so there is no
// connection for an MQTT last will to watch. Instead every entity goes
// unavailable once three polls in a row have gone unreported.
constexpr unsigned long EXPIRE_AFTER_S = POLL_INTERVAL_MS * 3UL / 1000UL;

enum class Topic { Usage, Device };

struct Entity {
  const char* component;
  const char* key;
  const char* name;
  Topic       topic;
  const char* valueTemplate;
  const char* deviceClass;
  const char* unit;
  const char* icon;
  int         precision;    // -1 leaves it to HA
  bool        measurement;
  bool        diagnostic;
};

// A null JSON value renders as "None", which HA's MQTT sensor treats as unknown.
constexpr Entity ENTITIES[] = {
  {"sensor", "session", "Session usage", Topic::Usage, "{{ value_json.session }}",
   nullptr, "%", "mdi:timer-sand", 0, true, false},
  {"sensor", "week", "Week usage", Topic::Usage, "{{ value_json.week }}",
   nullptr, "%", "mdi:calendar-week", 0, true, false},
  {"sensor", "session_resets", "Session resets", Topic::Usage,
   "{{ value_json.session_resets or none }}", "timestamp", nullptr, nullptr, -1, false, false},
  {"sensor", "week_resets", "Week resets", Topic::Usage,
   "{{ value_json.week_resets or none }}", "timestamp", nullptr, nullptr, -1, false, false},
  {"binary_sensor", "stale", "Usage stale", Topic::Usage,
   "{{ 'ON' if value_json.stale else 'OFF' }}", "problem", nullptr, nullptr, -1, false, true},
  {"sensor", "battery", "Battery", Topic::Device, "{{ value_json.battery }}",
   "battery", "%", nullptr, 0, true, false},
  {"sensor", "voltage", "Battery voltage", Topic::Device, "{{ value_json.voltage }}",
   "voltage", "V", nullptr, 2, true, true},
  {"binary_sensor", "usb", "USB power", Topic::Device,
   "{{ 'ON' if value_json.usb else 'OFF' }}", "plug", nullptr, nullptr, -1, false, false},
  {"sensor", "rssi", "Wi-Fi signal", Topic::Device, "{{ value_json.rssi }}",
   "signal_strength", "dBm", nullptr, 0, true, true},
};

// A discovery config is ~500 bytes; PubSubClient's 256-byte default would
// make the publish fail silently.
constexpr size_t MQTT_BUFFER = 1024;

// The close normally takes ~50 ms. A lost packet costs an lwIP retransmit,
// which the broker log put at ~3 s on this LAN, so wait a little longer than
// that before writing the poll off.
constexpr long CLOSE_TIMEOUT_MS = 3500;

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

char deviceId[20];
char usageTopic[40];
char deviceTopic[40];
bool discovered = false;

void initIds() {
  if (deviceId[0]) return;
  const uint64_t mac = ESP.getEfuseMac();
  // Low three MAC bytes, in the order the MAC is printed.
  snprintf(deviceId, sizeof(deviceId), "tokenmon-%02x%02x%02x",
           (unsigned)(mac >> 24) & 0xff, (unsigned)(mac >> 32) & 0xff,
           (unsigned)(mac >> 40) & 0xff);
  snprintf(usageTopic,  sizeof(usageTopic),  "%s/usage",  deviceId);
  snprintf(deviceTopic, sizeof(deviceTopic), "%s/device", deviceId);
  Serial.printf("[mqtt] device id %s\n", deviceId);
}

bool publishJson(const char* topic, const JsonDocument& doc, bool retained) {
  char payload[MQTT_BUFFER - 128];
  const size_t n = measureJson(doc);
  if (n >= sizeof(payload)) {
    Serial.printf("[mqtt] %s payload too large (%u)\n", topic, (unsigned)n);
    return false;
  }
  serializeJson(doc, payload, sizeof(payload));
  return mqtt.publish(topic, (const uint8_t*)payload, n, retained);
}

bool publishDiscovery(const Entity& e) {
  char uniqueId[48];
  snprintf(uniqueId, sizeof(uniqueId), "%s_%s", deviceId, e.key);

  JsonDocument doc;
  doc["name"]           = e.name;
  doc["unique_id"]      = uniqueId;
  doc["state_topic"]    = e.topic == Topic::Usage ? usageTopic : deviceTopic;
  doc["value_template"] = e.valueTemplate;
  doc["expire_after"]   = EXPIRE_AFTER_S;
  if (e.deviceClass) doc["device_class"]                = e.deviceClass;
  if (e.unit)        doc["unit_of_measurement"]         = e.unit;
  if (e.icon)        doc["icon"]                        = e.icon;
  if (e.precision >= 0) doc["suggested_display_precision"] = e.precision;
  if (e.measurement) doc["state_class"]                 = "measurement";
  if (e.diagnostic)  doc["entity_category"]             = "diagnostic";

  JsonObject dev = doc["device"].to<JsonObject>();
  dev["identifiers"].add(deviceId);
  dev["name"]         = HA_DEVICE_NAME;
  dev["manufacturer"] = "DIY";
  dev["model"]        = "TTGO T-Display ESP32";

  char topic[96];
  snprintf(topic, sizeof(topic), "%s/%s/%s/%s/config",
           HA_DISCOVERY_PREFIX, e.component, deviceId, e.key);
  // Retained, so HA picks the entities up again after its own restart.
  return publishJson(topic, doc, true);
}

bool connect() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(MQTT_BUFFER);
  // PubSubClient waits 15 s for CONNACK by default, and the radio stays on
  // all that time. The broker is on the LAN, so a second is plenty.
  mqtt.setSocketTimeout(1);
  wifiClient.setTimeout(1);

  // Like the HTTP request, the first attempt after association is
  // occasionally lost, and a retry is cheaper than skipping the poll.
  if (mqtt.connect(deviceId, MQTT_USER, MQTT_PASSWORD) ||
      mqtt.connect(deviceId, MQTT_USER, MQTT_PASSWORD)) {
    return true;
  }
  Serial.printf("[mqtt] connect failed, rc=%d\n", mqtt.state());
  return false;
}

// QoS 0 publishes return once lwIP has queued the bytes, and whatever it
// hasn't sent yet dies with the radio. Discovery is several KB, more than TCP
// sends before the first ACK, so this lost most of it in testing. The broker
// closes the connection only after reading DISCONNECT, and so everything
// before it, which makes its close the delivery receipt.
bool disconnectConfirmed() {
  static const uint8_t DISCONNECT[] = {0xE0, 0x00};
  wifiClient.write(DISCONNECT, sizeof(DISCONNECT));

  const int fd = wifiClient.fd();
  if (fd < 0) return false;
  fd_set readable;
  FD_ZERO(&readable);
  FD_SET(fd, &readable);
  timeval tv = {CLOSE_TIMEOUT_MS / 1000, (CLOSE_TIMEOUT_MS % 1000) * 1000};
  uint8_t b;
  const bool closed = select(fd + 1, &readable, nullptr, nullptr, &tv) > 0 &&
                      recv(fd, &b, 1, MSG_DONTWAIT) == 0;
  wifiClient.stop();
  return closed;
}

} // namespace

void publish(const BatteryReading& batt, int rssi, const UsageSnapshot* usage) {
  initIds();
  if (!connect()) return;

  // Discovery configs are retained on the broker, so once per boot is enough.
  bool discoveryOk = true;
  if (!discovered) {
    for (const Entity& e : ENTITIES) discoveryOk &= publishDiscovery(e);
  }

  // State is not retained: with expire_after set, HA would replay a retained
  // value as fresh after its own restart, however old it was.
  JsonDocument dev;
  // On USB the ADC reads the 4.9 V USB rail, not the cell, so neither the
  // voltage nor a percentage derived from it means anything.
  if (batt.charging) {
    dev["battery"] = nullptr;
    dev["voltage"] = nullptr;
  } else {
    dev["battery"] = batt.percent;
    dev["voltage"] = serialized(String(batt.millivolts / 1000.0f, 2));
  }
  dev["usb"]  = batt.charging;
  dev["rssi"] = rssi;
  bool ok = publishJson(deviceTopic, dev, false);

  if (usage) {
    JsonDocument u;
    u["session"]        = usage->sessionPct;
    u["week"]           = usage->weekPct;
    u["session_resets"] = usage->sessionResetsAt;
    u["week_resets"]    = usage->weekResetsAt;
    u["stale"]          = usage->stale;
    ok &= publishJson(usageTopic, u, false);
  }

  const bool delivered = disconnectConfirmed();
  if (!discovered) {
    discovered = discoveryOk && delivered;
    Serial.printf("[mqtt] discovery %s\n", discovered ? "ok" : "FAILED");
  }
  Serial.printf("[mqtt] published %s\n", ok && delivered ? "ok" : "FAILED");
}

} // namespace ha

#endif
