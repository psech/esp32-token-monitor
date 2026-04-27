# ESP32 Claude Code token monitor

<img width="4000" height="2252" alt="20260427_140343" src="https://github.com/user-attachments/assets/8af147ae-20df-44cd-af6a-c796f5717442" /><br />
A tiny dedicated dashboard that mirrors Claude Code's `/usage` panel — "Current session" and "Current week (all models)" — on a TTGO T-Display ESP32 sitting next to your laptop.

```
┌─────────────────────┐       ┌─────────────────────────┐       ┌──────────────────┐
│  TTGO T-Display     │  HTTP │  Node service in Docker │  HTTP │  Vue webpage     │
│  (Wi-Fi, 240×135)   │  ◄──  │  on the laptop          │  ──►  │  localhost:8080  │
└─────────────────────┘       └────────────┬────────────┘       └──────────────────┘
                                           │ HTTPS
                                           ▼
                              ┌──────────────────────────┐
                              │  api.anthropic.com       │
                              │  (rate-limit headers)    │
                              └──────────────────────────┘
```

## Repo layout

- [service/](service/) — Node 24 + Fastify, Dockerised. Reads the OAuth token from your macOS Keychain (via a helper script), makes a tiny `/v1/messages` call, and exposes `/api/usage` on the LAN.
- [firmware/](firmware/) — PlatformIO project for the TTGO T-Display. Polls the service, renders two progress bars + a 1 px countdown row.
- [webpage/](webpage/) — Vue 3 + Vite + Tailwind v4 dashboard at `http://localhost:8080`, mirrors the device with two bars, a radar countdown, and dark/light theme.

See each folder's README for details.

## Quick start

```
# One-time: shared Docker network for service ⇄ webpage
docker network create esp32-monitor-shared

# Service (laptop)
cd service
bin/refresh-token.sh
docker compose up --build

# Webpage (separate compose, talks to the service over the shared network)
cd ../webpage
docker compose up --build
# open http://localhost:8080

# Firmware (one-time)
cd ../firmware
cp include/secrets.h.example include/secrets.h
$EDITOR include/secrets.h     # Wi-Fi + laptop IP
pio run -t upload
pio device monitor
```

## Design notes

- Rate-limit data lives in **`anthropic-ratelimit-unified-*` response headers** on every `/v1/messages` call, not a separate endpoint. See [service/docs/usage-endpoint.md](service/docs/usage-endpoint.md).
- The service caches upstream responses for 60 s, so the ESP32's 10 s polls don't translate to 10 s upstream polls.
- All Wi-Fi creds and the service host live in `firmware/include/secrets.h` (gitignored). The OAuth credential blob lives in `service/.secrets/credentials.json` (also gitignored).
- Outbound HTTPS goes through Netskope; the corporate cert bundle is mounted into the Docker container.
- The webpage and service run as **separate compose stacks** on a manually-created external network (`esp32-monitor-shared`). The webpage's nginx reverse-proxies `/api/*` to the service container by name, so the browser never deals with CORS.

## Out of scope (v1)

WebSocket push, OTA updates, TLS on the LAN, multi-device, history persistence.

## Terms-of-use note

This project extracts the OAuth token Claude Code stores in the macOS Keychain and uses it to call `api.anthropic.com` from a separate process. **Whether that is compliant with Anthropic's terms is a gray area** — I'm not in a position to give a definitive answer.

What's known technically:

- The token has scope `user:inference` — designed for inference calls.
- The `anthropic-beta: oauth-2025-04-20` header suggests Anthropic has a recognized OAuth flow.
- Calls bill against your own subscription's rate-limit pool, not someone else's.
- Anthropic's product matrix puts subscription plans (Pro/Team/Max via claude.ai + Claude Code) on different commercial footing than the developer API (which uses dedicated API keys with usage-based billing).

What's potentially fraught:

- Subscription terms generally govern use of the *Claude Code product*. Pulling the OAuth token out and calling the API directly through your own service is technically not "using Claude Code" — even though it's your token, your account, your usage budget.
- Anthropic's [Usage Policy](https://www.anthropic.com/legal/aup) and the Commercial Terms attached to your subscription are authoritative — not anything reasoned from the technical mechanism.
- Sustained programmatic polling (even at 1/min) is a different posture than Claude Code's interactive use.

Suggested options, in increasing order of caution:

1. **Read the actual terms** linked from your Claude admin console + the [Usage Policy](https://www.anthropic.com/legal/aup). They override anything implied here.
2. **Ask Anthropic Support directly** — a short "we're building a personal monitoring dashboard, here's what it does, is this OK?" gets a real answer.
3. **Switch to a developer API key** — put a small amount of credit on a normal API account, use that token in `service/.secrets/credentials.json`. Unambiguously authorized; billed as developer usage (cents/month at this volume). Caveat: the rate-limit headers reflect API-tier limits, not your subscription's session/week — bars wouldn't match `/usage`.
4. **Drop the polling** — refresh only on a button press. Hard to argue that's abusive.

For a personal, non-commercial, you-own-the-token monitor, this is likely fine — but skim the terms or fire off a one-line support email before relying on it long-term.
