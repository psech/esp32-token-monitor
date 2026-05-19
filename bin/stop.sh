#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "[stop] bringing down webpage stack"
(cd "$REPO_ROOT/webpage" && docker compose down)

echo "[stop] bringing down service stack"
(cd "$REPO_ROOT/service" && docker compose down)

echo "[stop] done"
