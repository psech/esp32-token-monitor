# esp32-token-monitor — service

Tiny Node 24 + Fastify service that exposes Claude Code's rate-limit utilization on the LAN so the ESP32 firmware can render it as two progress bars.

See [docs/usage-endpoint.md](docs/usage-endpoint.md) for how the upstream call works.

## API

- `GET /healthz` → `{ ok: true }`
- `GET /api/usage` → live values, served from a 60 s in-memory cache
  ```json
  {
    "session": { "percent": 7,  "resetsAt": "2026-04-26T13:30:00.000Z", "status": "allowed" },
    "week":    { "percent": 1,  "resetsAt": "2026-05-03T18:00:00.000Z", "status": "allowed" },
    "fetchedAt": "2026-04-26T11:35:00.000Z",
    "stale": false
  }
  ```
  `stale: true` means the upstream call failed and the previous cached value is being served.

## One-time setup

1. **Install deps** (host, for `npm run dev`):
   ```
   npm install
   ```
2. **Extract OAuth credentials from the keychain** (host):
   ```
   bin/refresh-token.sh
   ```
   Writes `service/.secrets/credentials.json` (gitignored, mode 600). Re-run whenever `claude` rotates the token (the service logs a hint when this happens).

## Run locally (host)

```
npm run dev
curl localhost:8787/api/usage
```

## Run in Docker

```
bin/refresh-token.sh
docker compose up --build
curl localhost:8787/api/usage
```

The compose file mounts:
- `./.secrets/credentials.json` → `/run/secrets/credentials.json` (rw, so the service can rewrite on token refresh)
- `/usr/local/share/netskope-cert-bundle.pem` → `/etc/ssl/certs/netskope-ca.pem` (ro, picked up via `NODE_EXTRA_CA_CERTS`)

Without the cert mount, outbound HTTPS to `api.anthropic.com` fails with a self-signed-cert error (Netskope intercepts).

## Cost note

Each upstream refresh sends one ~10-token Haiku request. At a 60 s cache TTL that's ~1 call/min. Negligible against weekly limits, but not zero.

## Configuration

| Env var | Default | Notes |
| --- | --- | --- |
| `PORT` | `8787` | HTTP listen port |
| `HOST` | `0.0.0.0` | Bind address |
| `CREDENTIALS_FILE` | `./.secrets/credentials.json` | Path to the keychain-extracted blob |
| `UPSTREAM_CACHE_TTL_MS` | `60000` | How long to cache the rate-limit headers |
| `NODE_EXTRA_CA_CERTS` | _unset_ | Set to the Netskope cert path inside the container |
| `LOG_LEVEL` | `info` | Fastify logger level |
