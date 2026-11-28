#pragma once

#include "battery.h"
#include "net.h"

namespace ha {

// Publishes to Home Assistant over MQTT, using discovery so the device and its
// entities appear without any HA config. Call between net::up() and
// net::down(). `usage` is null when the fetch failed: the usage entities then
// keep their last value until expire_after marks them unavailable. A no-op
// unless MQTT_HOST is set in secrets.h.
void publish(const BatteryReading& batt, int rssi, const UsageSnapshot* usage);

} // namespace ha
