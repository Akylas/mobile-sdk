#!/bin/sh
# Generate the iOS demo project. Mirrors what scripts/android-dev gets from gradle: the SDK is
# built as a dependency of the app, not consumed as a prebuilt framework.
#
#   ./bootstrap.sh                  # arm64 simulator (the usual dev target)
#   ./bootstrap.sh device           # arm64 device
#   PROFILE=lite ./bootstrap.sh     # a different feature profile
#
# Re-run it after changing the profile or the platform. Day to day you do not: once the project
# exists, build from Xcode or with 'xcodebuild -project CartoDemo.xcodeproj'.
set -e

cd "$(dirname "$0")"
BASE_DIR=$(cd ../.. && pwd)
SWIG=${SWIG:-/Volumes/dev/carto/mobile-swig/swig}
# Matches scripts/android-dev/carto_mobile_sdk/build.gradle, so the two demos expose the same API.
PROFILE=${PROFILE:-standard+valhalla+geocoding+routing+packagemanager}

case "${1:-simulator}" in
  simulator) PLATFORM=SIMULATOR; SYSROOT=iphonesimulator; BASEARCH=arm64-simulator ;;
  device)    PLATFORM=OS;        SYSROOT=iphoneos;       BASEARCH=arm64 ;;
  *) echo "usage: $0 [simulator|device]" >&2; exit 1 ;;
esac
ARCH=arm64
BUILD_DIR="$BASE_DIR/build/ios_metal-$PLATFORM-$ARCH"

if [ ! -x "$SWIG" ]; then
  echo "SWIG executable not found at $SWIG - set SWIG=/path/to/mobile-swig/swig" >&2
  exit 1
fi

echo "==> Generating Objective-C bindings (profile: $PROFILE)"
(cd ../ && python3 swigpp-objc.py --profile "$PROFILE" --swig "$SWIG")

echo "==> Configuring the SDK Xcode project for $PLATFORM/$ARCH"
DEFINES=$(python3 - "$PROFILE" <<'PY'
import sys, os
sys.path.insert(0, os.path.join(os.getcwd(), '..'))
from build.sdk_build_utils import getProfile
print(' '.join('-D%s' % d for d in getProfile(sys.argv[1]).get('defines', '').split(';') if d))
PY
)
OPTIONS=$(python3 - "$PROFILE" <<'PY'
import sys, os
sys.path.insert(0, os.path.join(os.getcwd(), '..'))
from build.sdk_build_utils import getProfile
print(' '.join('-D%s' % o for o in getProfile(sys.argv[1]).get('cmake-options', '').split(';') if o))
PY
)

mkdir -p "$BUILD_DIR"
# shellcheck disable=SC2086
cmake -G Xcode $OPTIONS \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT="$SYSROOT" \
  -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DINCLUDE_OBJC:BOOL=ON \
  -DSINGLE_LIBRARY:BOOL=ON \
  -DSHARED_LIBRARY:BOOL=OFF \
  -DWRAPPER_DIR="$BASE_DIR/generated/ios-objc/proxies" \
  -DSDK_CPP_DEFINES="$DEFINES -D_CARTO_USE_METALANGLE -DZSTD_STATIC_LINKING_ONLY" \
  -DSDK_VERSION=Devel \
  -DSDK_PLATFORM=iOS \
  -DSDK_IOS_ARCH="$ARCH" \
  -DSDK_IOS_BASEARCH="$BASEARCH" \
  -S "$BASE_DIR/scripts/build" -B "$BUILD_DIR"

# CMake's Xcode generator still emits PBXBuildStyle objects (an Xcode 2 vestige that Xcode itself
# ignores), and XcodeGen's parser refuses to read a project containing them. Strip them so the SDK
# can be a real project dependency of the app rather than a prebuilt blob.
python3 - "$BUILD_DIR/carto_mobile_sdk.xcodeproj/project.pbxproj" <<'PYEOF'
import re, sys
path = sys.argv[1]
text = open(path).read()
text = re.sub(r'/\* Begin PBXBuildStyle section \*/.*?/\* End PBXBuildStyle section \*/\n', '', text, flags=re.S)
text = re.sub(r'\n\t*buildStyles = \(.*?\);', '', text, flags=re.S)
open(path, 'w').write(text)
print('stripped PBXBuildStyle from the SDK project')
PYEOF

# project.yml refers to the SDK project through this symlink, so switching platform is a
# re-bootstrap rather than an edit.
rm -f .sdkproj .angle
ln -s "$BUILD_DIR" .sdkproj
# MetalANGLE ships one static slice per arch; project.yml links it through this symlink.
ln -s "$BASE_DIR/libs-external/angle-metal/$BASEARCH" .angle

echo "==> Generating CartoDemo.xcodeproj"
xcodegen generate

echo
echo "Done. Open CartoDemo.xcodeproj, or:"
echo "  xcodebuild -project CartoDemo.xcodeproj -scheme CartoDemo -sdk $SYSROOT build"
