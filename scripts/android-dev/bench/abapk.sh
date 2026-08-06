#!/bin/sh
# Alternate two APKs to cancel device drift. $1 = apk, $2 = label, rest = extras.
ANDROID_SERIAL="${ANDROID_SERIAL:?set it to the device serial}"; export ANDROID_SERIAL
APK="$1"; LABEL="$2"; shift 2
adb install -r -t "$APK" >/dev/null 2>&1
/bin/sh ab.sh "$LABEL" "$@"
