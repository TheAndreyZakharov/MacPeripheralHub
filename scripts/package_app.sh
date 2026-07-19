#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_NAME="MacPeripheralHub.app"
RELEASE_APP="$ROOT_DIR/build/DerivedData/Build/Products/Release/$APP_NAME"
DIST_DIR="$ROOT_DIR/dist"
DIST_APP="$DIST_DIR/$APP_NAME"
ZIP_NAME="MacPeripheralHub.app.zip"
DIST_ZIP="$DIST_DIR/$ZIP_NAME"
CHECKSUM_FILE="$DIST_DIR/$ZIP_NAME.sha256"

"$ROOT_DIR/scripts/test_all.sh"

if [[ ! -d "$RELEASE_APP" ]]; then
  echo "Release app was not produced at: $RELEASE_APP" >&2
  exit 1
fi

mkdir -p "$DIST_DIR"
rm -rf "$DIST_APP"
rm -f "$DIST_ZIP"
rm -f "$CHECKSUM_FILE"
ditto "$RELEASE_APP" "$DIST_APP"
ditto -c -k --keepParent "$DIST_APP" "$DIST_ZIP"

(
  cd "$DIST_DIR"
  shasum -a 256 "$ZIP_NAME"
) > "$CHECKSUM_FILE"

echo "Built app:"
echo "$DIST_APP"
echo "Release archive:"
echo "$DIST_ZIP"
echo "Archive checksum:"
echo "$CHECKSUM_FILE"
