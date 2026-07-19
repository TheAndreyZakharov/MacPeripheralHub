#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_NAME="MacPeripheralHub.app"
RELEASE_APP="$ROOT_DIR/build/DerivedData/Build/Products/Release/$APP_NAME"
DIST_DIR="$ROOT_DIR/dist"
DIST_APP="$DIST_DIR/$APP_NAME"

"$ROOT_DIR/scripts/test_all.sh"

if [[ ! -d "$RELEASE_APP" ]]; then
  echo "Release app was not produced at: $RELEASE_APP" >&2
  exit 1
fi

mkdir -p "$DIST_DIR"
rm -rf "$DIST_APP"
ditto "$RELEASE_APP" "$DIST_APP"

echo "Built app:"
echo "$DIST_APP"
