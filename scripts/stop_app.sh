#!/usr/bin/env bash
set -euo pipefail

osascript -e 'tell application "MacPeripheralHub" to quit' >/dev/null 2>&1 || true
pkill -x "MacPeripheralHub" >/dev/null 2>&1 || true
