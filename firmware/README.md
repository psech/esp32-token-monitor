# esp32-token-monitor — firmware

PlatformIO project for the **TTGO T-Display ESP32** (240×135 ST7789 LCD). Polls the laptop service every `POLL_INTERVAL_MS` and draws two progress bars + a 1 px countdown row at the bottom.

## Layout

```
┌──────────────────────────────────────────────┐
│ Current session                          NN% │
│ ████████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │
│                                              │
│ Current week (all models)                NN% │
│ ████████████████████████░░░░░░░░░░░░░░░░░░░░ │
│ █████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ │ ← 1px countdown
└──────────────────────────────────────────────┘
```

A red 1 px line appears at the **top** when the most recent fetch failed or returned `stale: true`. It clears on the next successful fresh response.

Bar fill colour shifts based on percentage: magenta → amber at 75% → red at 90%.

## One-time setup

1. **Install PlatformIO** — either:
   - VS Code extension: `platformio.platformio-ide` (recommended, also installs the `pio` CLI), or
   - `pip install platformio` on the host.
2. **Create your secrets file**:
   ```
   cp include/secrets.h.example include/secrets.h
   $EDITOR include/secrets.h    # set WIFI_*, SERVICE_HOST, etc.
   ```
   `secrets.h` is gitignored.
3. Find the laptop's LAN IP and put it in `SERVICE_HOST`.

## Build, upload, monitor

```
pio run                    # compile
pio run -t upload          # flash over USB
pio device monitor         # serial @ 115200, with esp32_exception_decoder
```

Or, in VS Code: open the `firmware/` folder, then click PlatformIO's checkmark / arrow / plug icons in the bottom bar.

## Configuration knobs

All in `include/secrets.h`:

| Macro | Default | Notes |
| --- | --- | --- |
| `WIFI_SSID` | — | Wi-Fi network name |
| `WIFI_PASSWORD` | — | Wi-Fi password |
| `SERVICE_HOST` | — | IP/hostname of the laptop running the service |
| `SERVICE_PORT` | `8787` | HTTP port |
| `POLL_INTERVAL_MS` | `10000` | Fetch cadence; the countdown bar fills over this duration regardless of value |

## Files

- [platformio.ini](platformio.ini) — board, framework, TFT_eSPI pin config (no manual `User_Setup.h` editing)
- [src/main.cpp](src/main.cpp) — setup + loop, fetch scheduling, countdown ticking
- [src/net.cpp](src/net.cpp) — Wi-Fi + HTTP GET + ArduinoJson parse (filtered)
- [src/display.cpp](src/display.cpp) — bar/countdown/error rendering, layout constants
- [include/secrets.h.example](include/secrets.h.example) — template for `secrets.h`
