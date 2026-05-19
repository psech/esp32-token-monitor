#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if ! docker network inspect esp32-monitor-shared >/dev/null 2>&1; then
  echo "[start] creating docker network esp32-monitor-shared"
  docker network create esp32-monitor-shared >/dev/null
fi

echo "[start] refreshing OAuth token"
(cd "$REPO_ROOT/service" && ./bin/refresh-token.sh)

echo "[start] bringing up service stack"
(cd "$REPO_ROOT/service" && docker compose up --build --detach)

echo "[start] bringing up webpage stack"
(cd "$REPO_ROOT/webpage" && docker compose up --build --detach)

echo "[start] done — service on :8787, webpage on :8080"
