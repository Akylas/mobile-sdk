#!/bin/bash
# A/B benchmark for the demo: two intent-extra configurations, alternated, N repetitions each.
#
# The metric is PROCESS CPU TIME over a fixed window after launch (/proc/<pid>/stat utime+stime),
# which is what tile generation costs - frame timings from a loading map are worthless, and a single
# run is dominated by whether the tile caches were warm (a cold DEM cache once made a 10% difference
# look like 34%). Alternating A/B/A/B is what makes the comparison mean anything.
#
#   ./bench-ab.sh                                  # contour interval ladder: new defaults vs old
#   A_EXTRA="--es routeJoin miter" B_EXTRA="--es routeJoin round" ./bench-ab.sh
#   DEVICE=emulator-5554 REPS=5 WINDOW=40 ./bench-ab.sh
set -u

DEVICE=${DEVICE:-$(adb devices | awk 'NR==2 {print $1}')}
REPS=${REPS:-3}
WINDOW=${WINDOW:-25}
PKG=${PKG:-com.massifmaps.test}
BASE=${BASE:---es map false --es contourLayer true --es hillshade true --es terrain true --es ui false --es lon 5.7606 --es lat 45.2440 --es zoom 10.5 --es tilt 45}
A_LABEL=${A_LABEL:-A}
B_LABEL=${B_LABEL:-B}
A_EXTRA=${A_EXTRA:-}
B_EXTRA=${B_EXTRA:---es contourLadder 9:10,13:5,-1:1 --es contourResLadder -1:96}

run() { # $1 label, $2 extras
    adb -s "$DEVICE" shell am force-stop $PKG >/dev/null
    adb -s "$DEVICE" shell am start -n $PKG/.MainActivity $BASE $2 >/dev/null 2>&1
    local pid=""
    for _ in $(seq 1 40); do pid=$(adb -s "$DEVICE" shell pidof $PKG | tr -d '\r'); [ -n "$pid" ] && break; done
    local c0=$(adb -s "$DEVICE" shell cat /proc/$pid/stat 2>/dev/null | awk '{print $14+$15}')
    sleep "$WINDOW"
    local c1=$(adb -s "$DEVICE" shell cat /proc/$pid/stat | awk '{print $14+$15}')
    echo "$1 cpu_s=$(echo "scale=2; ($c1-$c0)/100" | bc)"   # 100 ticks = 1 CPU-second
}

echo "device=$DEVICE reps=$REPS window=${WINDOW}s"
for rep in $(seq 1 "$REPS"); do
    run "$A_LABEL rep$rep" "$A_EXTRA"
    run "$B_LABEL rep$rep" "$B_EXTRA"
done
