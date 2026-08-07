#!/bin/sh
# Interleaved A/B of the shading DEM detail level (elevation levels beyond the mesh cap).
# One APK; the level is a system property read once per process, so every arm restarts the app.
# Prints the PROF lines tagged by arm, plus the elevation-texture pipeline counters.
# $1 = rounds (default 2), rest = extra intent extras for north.sh.
ANDROID_SERIAL="${ANDROID_SERIAL:?set it to the device serial}"; export ANDROID_SERIAL
ROUNDS="${1:-2}"; shift 2>/dev/null
DIR=$(dirname "$0")
i=0
while [ "$i" -lt "$ROUNDS" ]; do
  for level in 0 1 2; do
    adb shell setprop debug.carto.paintdetail "$level"
    sh "$DIR/north.sh" "detail$level" "$@"
    echo "--- dem pipeline (detail$level):"
    adb logcat -d -s carto-mobile-sdk | grep "RenderStats: dem " | tail -3
  done
  i=$((i + 1))
done
