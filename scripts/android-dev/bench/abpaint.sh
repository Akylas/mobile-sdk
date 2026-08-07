#!/bin/sh
# Interleaved A/B of the terrain paint against the normal-map hillshade, one APK.
# Three arms: paint at the mesh DEM level, paint at full DEM detail, and the normal map.
# $1 = rounds (default 3), rest = extra intent extras for north.sh.
# Feed the output to absum.py for medians.
ANDROID_SERIAL="${ANDROID_SERIAL:?set it to the device serial}"; export ANDROID_SERIAL
ROUNDS="${1:-3}"; shift 2>/dev/null
DIR=$(dirname "$0")
i=0
while [ "$i" -lt "$ROUNDS" ]; do
  adb shell setprop debug.carto.terrainpaint 1; adb shell setprop debug.carto.paintdetail 0
  sh "$DIR/north.sh" paint "$@"
  adb shell setprop debug.carto.terrainpaint 1; adb shell setprop debug.carto.paintdetail 1
  sh "$DIR/north.sh" paint-detail "$@"
  adb shell setprop debug.carto.terrainpaint 0; adb shell setprop debug.carto.paintdetail 0
  sh "$DIR/north.sh" normalmap "$@"
  i=$((i + 1))
done
