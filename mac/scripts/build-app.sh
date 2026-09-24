#!/usr/bin/env bash
# Builds FlipDeck.app from the SwiftPM package. A real bundle is required for
# the Bluetooth permission prompt (NSBluetoothAlwaysUsageDescription) and for
# Mac notifications. Usage: scripts/build-app.sh [release|debug]
set -euo pipefail
cd "$(dirname "$0")/.."

CONFIG="${1:-release}"
swift build -c "$CONFIG" --product FlipDeck
BIN_DIR="$(swift build -c "$CONFIG" --show-bin-path)"

APP="build/FlipDeck.app"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp "$BIN_DIR/FlipDeck" "$APP/Contents/MacOS/FlipDeck"
cp scripts/Info.plist "$APP/Contents/Info.plist"

# Ad-hoc signature so macOS attributes Bluetooth/notification permissions to
# this app. Replace "-" with a Developer ID to distribute.
codesign --force --sign - --timestamp=none "$APP"
echo "Built $(pwd)/$APP"
