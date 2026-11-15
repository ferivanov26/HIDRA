#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "${1:-}" != "--initialize-empty" || -z "${2:-}" ]]; then
    echo "Uso: bash scripts/prepare_littlefs.sh --initialize-empty /dev/ttyACM0" >&2
    echo "Somente no primeiro uso: substitui todo o diário LittleFS da placa." >&2
    exit 2
fi
pio run -d firmware -e esp32dev -t buildfs
pio run -d firmware -e esp32dev -t uploadfs --upload-port "$2"
