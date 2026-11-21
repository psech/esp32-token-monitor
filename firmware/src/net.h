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

} // namespace net
