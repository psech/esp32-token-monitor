#pragma once

#include <stdbool.h>

struct UsageSnapshot {
  int sessionPct;
  int weekPct;
  bool stale;
};

namespace net {

// Powers the radio up, connects, GETs /api/usage, parses it and powers the
// radio back down. The radio is off between calls to save battery. Returns
// true on success and fills `out`. Logs to Serial on failure.
bool fetchUsage(UsageSnapshot& out);

// Signal strength in dBm sampled when the last fetchUsage() connected, or 0
// if it failed to connect. The radio is off between polls, so this is the
// freshest value there is.
int lastRssi();

} // namespace net
