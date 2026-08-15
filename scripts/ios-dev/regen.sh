#!/bin/sh
# Regenerate MassifDemo.xcodeproj. Run this after adding a source file.
#
# The strip is not a one-off: CMake re-emits PBXBuildStyle (an Xcode 2 vestige that Xcode ignores
# and XcodeGen's parser refuses) every time it reconfigures, which happens on any CMakeLists
# change through the ZERO_CHECK phase. Without stripping it again here, 'xcodegen generate' fails
# and - because it fails after writing nothing - the project silently keeps the old file list.
set -e
cd "$(dirname "$0")"

PBXPROJ=".sdkproj/massif.xcodeproj/project.pbxproj"
if [ ! -f "$PBXPROJ" ]; then
  echo "No SDK project at $PBXPROJ - run ./bootstrap.sh first" >&2
  exit 1
fi

python3 - "$PBXPROJ" <<'PYEOF'
import re, sys
path = sys.argv[1]
text = open(path).read()
stripped = re.sub(r'/\* Begin PBXBuildStyle section \*/.*?/\* End PBXBuildStyle section \*/\n', '', text, flags=re.S)
stripped = re.sub(r'\n\t*buildStyles = \(.*?\);', '', stripped, flags=re.S)
if stripped != text:
    open(path, 'w').write(stripped)
    print('stripped PBXBuildStyle from the SDK project')
PYEOF

xcodegen generate
