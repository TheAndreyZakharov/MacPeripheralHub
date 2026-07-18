#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_PATH="$ROOT_DIR/build/DerivedData/Build/Products/Debug/MacPeripheralHub.app"

"$ROOT_DIR/scripts/build_app.sh"
open "$APP_PATH"
