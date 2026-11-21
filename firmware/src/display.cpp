#include "display.h"

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <driver/ledc.h>
#include <esp_sleep.h>

#include "secrets.h"

#ifndef BACKLIGHT_PERCENT
#define BACKLIGHT_PERCENT 30
#endif

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

// Status row: Wi-Fi in the left half, battery in the right half.
constexpr int STATUS_ROW_Y  = 108;
constexpr int STATUS_ROW_H  = 18;
constexpr int STATUS_HALF_W = BAR_W / 2;

// Battery, right-aligned: "NN%" text then a 22x11 icon with a 2x5 nub.
constexpr int BATT_ICON_W   = 22;
constexpr int BATT_ICON_H   = 11;
constexpr int BATT_NUB_W    = 2;
constexpr int BATT_NUB_H    = 5;
constexpr int BATT_ICON_X   = PAD_X + BAR_W - BATT_NUB_W - BATT_ICON_W;
constexpr int BATT_ICON_Y   = STATUS_ROW_Y + 3;
constexpr int BATT_TEXT_GAP = 6;

// Wi-Fi, left-aligned: four rising signal bars then "-NN dBm" text. Bars share
// the battery icon's bottom edge so the row reads as one line.
constexpr int WIFI_BARS     = 4;
constexpr int WIFI_BAR_W    = 3;
constexpr int WIFI_BAR_GAP  = 2;
constexpr int WIFI_BAR_STEP = 3;
constexpr int WIFI_BOTTOM_Y = BATT_ICON_Y + BATT_ICON_H - 1;
constexpr int WIFI_ICON_W   = WIFI_BARS * WIFI_BAR_W + (WIFI_BARS - 1) * WIFI_BAR_GAP;
constexpr int WIFI_TEXT_GAP = 6;

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
constexpr uint16_t COLOR_CHARGING    = 0x3666;       // soft green

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

// The backlight is the largest constant load on battery. LEDC's default APB
// clock is gated in light sleep, which would freeze the pin at whatever level
// it happened to be; the RTC 8 MHz oscillator keeps PWM running through sleep.
void initBacklight() {
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC8M, ESP_PD_OPTION_ON);

  ledc_timer_config_t timer = {};
  timer.speed_mode      = LEDC_LOW_SPEED_MODE;
  timer.duty_resolution = LEDC_TIMER_8_BIT;
  timer.timer_num       = LEDC_TIMER_0;
  timer.freq_hz         = 5000;
  timer.clk_cfg         = LEDC_USE_RTC8M_CLK;
  ledc_timer_config(&timer);

  int pct = BACKLIGHT_PERCENT;
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;

  ledc_channel_config_t channel = {};
  channel.gpio_num   = TFT_BL;
  channel.speed_mode = LEDC_LOW_SPEED_MODE;
  channel.channel    = LEDC_CHANNEL_0;
  channel.timer_sel  = LEDC_TIMER_0;
  channel.duty       = (255 * pct) / 100;
  ledc_channel_config(&channel);
}

} // namespace

namespace display {

void begin() {
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(COLOR_BG);
  initBacklight();
}

void drawBars(int sessionPct, int weekPct) {
  // Clear everything between the top error indicator and the battery row —
  // wipes any residual splash text and avoids overlap when label/bar Y
  // positions don't perfectly align.
  tft.fillRect(0, 1, SCREEN_W, STATUS_ROW_Y - 1, COLOR_BG);

  drawLabelAndPercent(SESSION_LBL_Y, "Current session",        sessionPct);
  drawBar(SESSION_BAR_Y, sessionPct);

  drawLabelAndPercent(WEEK_LBL_Y,    "Current week (all models)", weekPct);
  drawBar(WEEK_BAR_Y, weekPct);
}

void drawBattery(int percent, bool charging) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;

  tft.fillRect(PAD_X + STATUS_HALF_W, STATUS_ROW_Y, BAR_W - STATUS_HALF_W, STATUS_ROW_H, COLOR_BG);

  uint16_t color = COLOR_LABEL;
  if (charging)          color = COLOR_CHARGING;
  else if (percent < 10) color = COLOR_FILL_DANGER;
  else if (percent < 20) color = COLOR_FILL_WARN;

  const int x = BATT_ICON_X;
  const int y = BATT_ICON_Y;
  tft.drawRect(x, y, BATT_ICON_W, BATT_ICON_H, color);
  tft.fillRect(x + BATT_ICON_W, y + (BATT_ICON_H - BATT_NUB_H) / 2,
               BATT_NUB_W, BATT_NUB_H, color);

  if (charging) {
    // Lightning bolt: two triangles meeting in the middle of the icon.
    const int cx = x + BATT_ICON_W / 2;
    const int cy = y + BATT_ICON_H / 2;
    tft.fillTriangle(cx + 2, y + 1, cx - 4, cy + 1, cx + 1, cy + 1, color);
    tft.fillTriangle(cx - 1, cy - 1, cx + 4, cy - 1, cx - 2, y + BATT_ICON_H - 2, color);
  } else {
    const int innerW = BATT_ICON_W - 4;
    const int filled = (innerW * percent + 50) / 100;
    if (filled > 0) tft.fillRect(x + 2, y + 2, filled, BATT_ICON_H - 4, color);
  }

  char buf[12];
  if (charging) snprintf(buf, sizeof(buf), "Charging");
  else          snprintf(buf, sizeof(buf), "%d%%", percent);
  tft.setTextFont(2);
  tft.setTextColor(color, COLOR_BG);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(buf, x - BATT_TEXT_GAP, STATUS_ROW_Y);
}

void drawWifi(int rssi) {
  tft.fillRect(PAD_X, STATUS_ROW_Y, STATUS_HALF_W, STATUS_ROW_H, COLOR_BG);

  const bool connected = rssi != 0;
  int lit = 0;
  if (connected) {
    if      (rssi >= -55) lit = 4;
    else if (rssi >= -65) lit = 3;
    else if (rssi >= -75) lit = 2;
    else                  lit = 1;
  }

  for (int i = 0; i < WIFI_BARS; i++) {
    const int h = WIFI_BAR_STEP * (i + 1);
    const int x = PAD_X + i * (WIFI_BAR_W + WIFI_BAR_GAP);
    const int y = WIFI_BOTTOM_Y - h + 1;
    if (connected) tft.fillRect(x, y, WIFI_BAR_W, h, i < lit ? COLOR_LABEL : COLOR_TRACK);
    else           tft.drawRect(x, y, WIFI_BAR_W, h, COLOR_ERROR);
  }

  char buf[16];
  if (connected) snprintf(buf, sizeof(buf), "%d dBm", rssi);
  else           snprintf(buf, sizeof(buf), "No Wi-Fi");
  tft.setTextFont(2);
  tft.setTextColor(connected ? COLOR_LABEL : COLOR_ERROR, COLOR_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(buf, PAD_X + WIFI_ICON_W + WIFI_TEXT_GAP, STATUS_ROW_Y);
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
