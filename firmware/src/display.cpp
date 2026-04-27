#include "display.h"

#include <Arduino.h>
#include <TFT_eSPI.h>

namespace {

TFT_eSPI tft;

constexpr int SCREEN_W = 240;
constexpr int SCREEN_H = 135;

// Layout (landscape, rotation 1).
constexpr int PAD_X         = 10;
constexpr int BAR_W         = SCREEN_W - PAD_X * 2;       // 220
constexpr int BAR_H         = 14;

constexpr int SESSION_LBL_Y = 14;
constexpr int SESSION_BAR_Y = 32;

constexpr int WEEK_LBL_Y    = 60;
constexpr int WEEK_BAR_Y    = 78;

constexpr int COUNTDOWN_Y   = SCREEN_H - 1;   // 134
constexpr int ERROR_Y       = 0;

// Colors (16-bit 565).
constexpr uint16_t COLOR_BG          = TFT_BLACK;
constexpr uint16_t COLOR_LABEL       = 0xC618;       // light grey
constexpr uint16_t COLOR_TRACK       = 0x2104;       // very dim grey
constexpr uint16_t COLOR_FILL        = 0xC81F;       // magenta-ish (matches /usage vibe)
constexpr uint16_t COLOR_FILL_WARN   = 0xFD20;       // amber
constexpr uint16_t COLOR_FILL_DANGER = 0xF800;       // red
constexpr uint16_t COLOR_COUNTDOWN   = 0x4208;       // dim slate
constexpr uint16_t COLOR_ERROR       = TFT_RED;

uint16_t fillColor(int pct) {
  if (pct >= 90) return COLOR_FILL_DANGER;
  if (pct >= 75) return COLOR_FILL_WARN;
  return COLOR_FILL;
}

void drawLabelAndPercent(int y, const char* label, int pct) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setTextFont(2);
  tft.fillRect(PAD_X, y, BAR_W, 16, COLOR_BG);
  tft.drawString(label, PAD_X, y);

  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(buf, PAD_X + BAR_W, y);
}

void drawBar(int y, int pct) {
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  int filled = (BAR_W * pct) / 100;
  uint16_t c = fillColor(pct);

  tft.fillRect(PAD_X, y, filled, BAR_H, c);
  tft.fillRect(PAD_X + filled, y, BAR_W - filled, BAR_H, COLOR_TRACK);
}

} // namespace

namespace display {

void begin() {
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(COLOR_BG);
}

void drawBars(int sessionPct, int weekPct) {
  // Clear the full content band between the top error indicator and the
  // bottom countdown row — wipes any residual splash text and avoids
  // overlap when label/bar Y positions don't perfectly align.
  tft.fillRect(0, 1, SCREEN_W, COUNTDOWN_Y - 1, COLOR_BG);

  drawLabelAndPercent(SESSION_LBL_Y, "Current session",        sessionPct);
  drawBar(SESSION_BAR_Y, sessionPct);

  drawLabelAndPercent(WEEK_LBL_Y,    "Current week (all models)", weekPct);
  drawBar(WEEK_BAR_Y, weekPct);
}

void drawCountdown(float progress) {
  if (progress < 0.0f) progress = 0.0f;
  if (progress > 1.0f) progress = 1.0f;
  int filled = (int)(SCREEN_W * progress);
  if (filled > 0)            tft.drawFastHLine(0, COUNTDOWN_Y, filled, COLOR_COUNTDOWN);
  if (filled < SCREEN_W)     tft.drawFastHLine(filled, COUNTDOWN_Y, SCREEN_W - filled, COLOR_BG);
}

void setErrorIndicator(bool error) {
  tft.drawFastHLine(0, ERROR_Y, SCREEN_W, error ? COLOR_ERROR : COLOR_BG);
}

void drawSplash(const char* line1, const char* line2) {
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_LABEL, COLOR_BG);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(line1, SCREEN_W / 2, SCREEN_H / 2 - 10);
  if (line2 && line2[0]) tft.drawString(line2, SCREEN_W / 2, SCREEN_H / 2 + 10);
}

} // namespace display
