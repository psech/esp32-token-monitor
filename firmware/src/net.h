#pragma once

#include <stdbool.h>

struct UsageSnapshot {
  int sessionPct;
  int weekPct;
  bool stale;
};

namespace net {

void connectWiFi();   // blocks until connected (with retries)
bool isOnline();

// Performs HTTP GET to /api/usage on the configured host:port and parses
// the JSON. Returns true on success and fills `out`. Logs to Serial on failure.
bool fetchUsage(UsageSnapshot& out);

} // namespace net
