#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

"$ROOT_DIR/scripts/test_core.sh"

xcodebuild \
  -project "$ROOT_DIR/MacPeripheralHub.xcodeproj" \
  -scheme "MacPeripheralHub" \
  -configuration "Release" \
  -derivedDataPath "$ROOT_DIR/build/DerivedData" \
  build
