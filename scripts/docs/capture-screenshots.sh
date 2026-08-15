#!/usr/bin/env bash
# Capture feature screenshots (and an optional video) from the Android demo app
# (scripts/android-dev) into website/static/img/features/.
#
# The demo's terrain/hillshade/contour setup (SecondFragment.addCompositeMap /
# addTerrain) streams its data from public online tiles — a terrarium DEM
# (tiles.mapterhorn.com) and a vector basemap (tiles.openfreemap.org) — so the
# emulator only needs internet; no map data has to be pushed to the device.
#
# The native .so are prebuilt under massif/, so the app builds in seconds
# (no NDK compile). Requires a booted emulator / connected device (adb) and ffmpeg
# for cropping/encoding.
#
# Usage:
#   scripts/docs/capture-screenshots.sh [name]     # still -> features/<name>.png
#   RECORD=1 scripts/docs/capture-screenshots.sh    # also record a ~14s video
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
DEV="$ROOT/scripts/android-dev"
OUT="$ROOT/website/static/img/features"
APP_ID="${APP_ID:-com.massifmaps.MassifDemo}"
NAME="${1:-feature}"
mkdir -p "$OUT"

command -v adb >/dev/null || { echo "adb not found (install Android platform-tools)"; exit 1; }
adb get-state >/dev/null 2>&1 || { echo "No device/emulator. Boot one: emulator -avd <name>"; exit 1; }

echo "==> Building demo (assembleDebug, prebuilt native, offline)"
( cd "$DEV" && ./gradlew :app:assembleDebug -x lint --offline )
APK="$(find "$DEV/app/build/outputs/apk/debug" -name '*.apk' | head -1)"
[ -n "$APK" ] || { echo "APK not found"; exit 1; }

echo "==> Installing + launching"
adb install -r -g "$APK" >/dev/null
adb shell am force-stop "$APP_ID"
adb shell monkey -p "$APP_ID" -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1
until [ -n "$(adb shell pidof "$APP_ID" 2>/dev/null)" ]; do sleep 1; done
echo "   waiting for online tiles to render…"; sleep 11

echo "==> Screenshot -> $OUT/$NAME.png"
adb exec-out screencap -p > "$OUT/$NAME.png"

if [ "${RECORD:-0}" = "1" ]; then
  echo "==> Recording ~14s video"
  adb shell screenrecord --bit-rate 8000000 --time-limit 14 /sdcard/${NAME}.mp4 &
  REC=$!; sleep 1
  # pan / drag gestures for some motion
  adb shell input swipe 540 1400 540 700 1600
  adb shell input swipe 300 1000 800 1000 1600
  adb shell input swipe 800 1100 300 900 1600
  adb shell input swipe 540 700 540 1400 1600
  wait $REC 2>/dev/null
  adb pull /sdcard/${NAME}.mp4 "$OUT/${NAME}.mp4"
fi

# Crop the Android status/app bars + nav bar and encode for web (needs ffmpeg).
# Android phone screenshots here are 1080x2400; adjust the crop for other devices.
if command -v ffmpeg >/dev/null; then
  echo "==> Cropping chrome + encoding (ffmpeg)"
  ffmpeg -y -loglevel error -i "$OUT/$NAME.png" \
    -vf "crop=1080:2055:0:195" -q:v 3 "$OUT/$NAME.jpg" && rm -f "$OUT/$NAME.png"
  if [ -f "$OUT/${NAME}.mp4" ]; then
    ffmpeg -y -loglevel error -i "$OUT/${NAME}.mp4" \
      -vf "crop=1080:2055:0:195,scale=640:-2" -c:v libx264 -pix_fmt yuv420p \
      -movflags +faststart -crf 27 -an "$OUT/${NAME}-web.mp4"
  fi
else
  echo "   (install ffmpeg to auto-crop the status/nav bars and encode the video)"
fi

echo "done. Review $OUT/ and point the feature doc's image/video at it."
echo
echo "Tip: for distinct feature shots, edit scripts/android-dev SecondFragment"
echo "     (camera setFocusPos/setZoom/setTilt; Massif tilt 90=top-down, low=horizon)"
echo "     and comment addTerrainControls to hide the debug UI. Restore it after."
