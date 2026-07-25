#!/usr/bin/env bash
# Capture feature screenshots from the Android demo app (scripts/android-dev) into
# website/static/img/features/. Requires a booted emulator or a connected device
# (adb) and the demo's map data present on the device.
#
# The demo (SecondFragment) already exercises 3D terrain / hillshade / contours /
# composite layers; this script builds it, installs it, launches it and pulls a
# framebuffer grab. Feature captures need real data on the device:
#   - a vector style at $STYLE_PATH (default /sdcard/alpimaps_mbtiles/osm.zip)
#   - an RGB DEM source reachable by the demo
# Set those up first (adb push), then run this.
#
# Usage:
#   scripts/docs/capture-screenshots.sh [name]
#     name  optional output basename (default: screenshot-<timestamp-less index>)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DEV="$ROOT/scripts/android-dev"
OUT="$ROOT/website/static/img/features"
APP_ID="${APP_ID:-com.akylas.cartotest}"
STYLE_PATH="${STYLE_PATH:-/sdcard/alpimaps_mbtiles/osm.zip}"
mkdir -p "$OUT"

command -v adb >/dev/null || { echo "adb not found (install Android platform-tools)"; exit 1; }
adb get-state >/dev/null 2>&1 || { echo "No device/emulator. Boot one first (adb devices)."; exit 1; }

echo "==> Building demo (assembleDebug, native .so are prebuilt under carto_mobile_sdk/)"
( cd "$DEV" && ./gradlew :app:assembleDebug -x lint )

APK="$(find "$DEV/app/build/outputs/apk/debug" -name '*.apk' | head -1)"
[ -n "$APK" ] || { echo "APK not found"; exit 1; }
echo "==> Installing $APK"
adb install -r "$APK" >/dev/null

echo "==> Data check: $STYLE_PATH"
adb shell "[ -f '$STYLE_PATH' ]" && echo "   style present" || echo "   WARNING: style missing — map may be empty"

echo "==> Launching"
adb shell monkey -p "$APP_ID" -c android.intent.category.LAUNCHER 1 >/dev/null
echo "   waiting for tiles to render…"; sleep 8

NAME="${1:-feature}"
DEST="$OUT/$NAME.png"
echo "==> Capturing -> $DEST"
adb exec-out screencap -p > "$DEST"
echo "done. Review $DEST and, if good, point the feature doc's image at it."
echo
echo "Tip: to script several shots, drive the demo to each feature (edit SecondFragment"
echo "     focus / addMap call) and re-run with a name: capture-screenshots.sh terrain-hero"
