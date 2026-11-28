#pragma once

#include <stdbool.h>

struct UsageSnapshot {
  int sessionPct;
  int weekPct;
  char sessionResetsAt[32];
  char weekResetsAt[32];
  bool stale;
};

namespace net {

// The radio is off between polls to save battery. A poll brackets all of its
// network work between up() and down(). up() returns false if it couldn't
// join the network; down() must be called either way.
bool up();
void down();

// GETs /api/usage and parses it. Requires up(). Returns true on success and
// fills `out`. Logs to Serial on failure.
bool fetchUsage(UsageSnapshot& out);

// Signal strength in dBm sampled when the last up() connected, or 0 if it
// failed to connect. The radio is off between polls, so this is the freshest
// value there is.
int lastRssi();

} // namespace net
