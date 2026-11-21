#pragma once

#include <stdint.h>

namespace display {

void begin();

// Redraw both bars + their labels/percent. Call only when values change.
// percentages are clamped to 0..100.
void drawBars(int sessionPct, int weekPct);

// Battery row above the countdown bar: "NN%" plus a filled icon while on
// battery, a green bolt and "Charging" while on USB.
void drawBattery(int percent, bool charging);

// Redraw the 1px countdown bar at the bottom edge.
// progress is 0..1 (fraction of poll interval elapsed).
void drawCountdown(float progress);

// Persistent 1px indicator at the very top edge.
// red = error/stale, otherwise hidden.
void setErrorIndicator(bool error);

// Draw a "connecting…" splash before first successful fetch.
void drawSplash(const char* line1, const char* line2);

} // namespace display
