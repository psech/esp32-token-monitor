# esp32-token-monitor — webpage

<img width="671" height="353" alt="Screenshot 2026-04-27 at 4 21 24 pm" src="https://github.com/user-attachments/assets/c3c87c13-be55-4014-9c38-58fbbe80423c" /><br />
Vue 3 + Vite + Tailwind v4 dashboard that mirrors the ESP32 LCD on the desktop. Two progress bars, a radar countdown, and a dark/light theme. Lives at `http://localhost:8080`.

## Layout

```
┌──────────────────────────────────────────────┐
│ ESP32 TOKEN MONITOR                          │
│                                              │
│ CURRENT SESSION                          8%  │
│ ████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │
│ resets at 23:30                              │
│                                              │
│ CURRENT WEEK (ALL MODELS)                 2% │
│ ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░  │
│ resets May 1 08:00                           │
│                                              │
│                                       ╭──╮   │
│  [STALE DATA ▾]           [☾ theme]   │ ╱│   │  ← radar (60×60)
│                                       ╰──╯   │
└──────────────────────────────────────────────┘
```

- Bar fill: green → amber at 75% → red at 90% (mirrors firmware).
- Dark theme adds a subtle neon glow under each filled bar; light theme drops it.
- Radar arm completes one rotation per `POLL_INTERVAL_MS` (10 s). A small blip pulses on each successful fetch.
- `STALE DATA ▾` chip appears when the last fetch failed or the service returned `stale: true`. Click to expand the upstream error.
- Theme toggle cycles `auto → dark → light → auto`. `auto` follows `prefers-color-scheme`. Choice persists in `localStorage["esp32-monitor.theme"]`.

## Architecture

Static SPA built by Vite, served by nginx in a Docker container:

```
browser → http://localhost:8080
            │
            ▼
        ┌─── nginx (in webpage container) ───┐
        │  /                  → static dist/ │
        │  /api/*  →  monitor:8787/api/*     │
        └────────────────────────────────────┘
                       │ shared network: esp32-monitor-shared
                       ▼
                 service container
```

The webpage and service are in **separate compose stacks**. They communicate over the manually-created external network `esp32-monitor-shared`. The browser only ever talks to `localhost:8080` → no CORS.

## One-time setup

```bash
docker network create esp32-monitor-shared
```

(Skip if the service stack already created it. `docker compose up` will print a clear error if the network is missing.)

## Run in Docker

```bash
docker compose up --build
# open http://localhost:8080
```

The service stack must be running too — otherwise the page shows the stale badge with `HTTP 502`.

## Run locally for development

```bash
nvm use                       # node 24.15
npm install
npm run dev                   # http://127.0.0.1:5173
```

Vite proxies `/api/*` to whatever `VITE_API_BASE` points at (default `http://localhost:8787`, i.e. the service running on the host).

## Configuration knobs

| Env var | Default | Where | Notes |
| --- | --- | --- | --- |
| `VITE_API_BASE` | `http://localhost:8787` | dev only | Where Vite's dev-server proxies `/api`. Not used in the Docker build. |
| Poll interval | `10000` ms | [src/composables/useUsage.ts](src/composables/useUsage.ts) | Change this constant if you also change the radar's "rotation per poll" intuition. |

## Files

- [src/App.vue](src/App.vue) — page layout
- [src/components/UsageBar.vue](src/components/UsageBar.vue) — label + percent + fill + "resets at"
- [src/components/Radar.vue](src/components/Radar.vue) — SVG sweep arm + blip pulse
- [src/components/StaleBadge.vue](src/components/StaleBadge.vue) — collapsible stale chip
- [src/components/ThemeToggle.vue](src/components/ThemeToggle.vue) — auto/dark/light cycle button
- [src/composables/useUsage.ts](src/composables/useUsage.ts) — 10 s polling, stale tracking
- [src/composables/useTheme.ts](src/composables/useTheme.ts) — theme mode + system preference
- [src/style.css](src/style.css) — Tailwind v4 entry + theme tokens (`@theme { … }`)
- [nginx.conf](nginx.conf) — static + `/api/` proxy
- [Dockerfile](Dockerfile) — multi-stage: node build → nginx serve
- [docker-compose.yml](docker-compose.yml) — `127.0.0.1:8080:80`, shared network
