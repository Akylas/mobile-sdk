import os
import sys
import shutil
import argparse
from build.sdk_build_utils import *

def gradle(args, dir, *cmdArgs):
    return execute(args.gradle, dir, *cmdArgs)

def buildRoutingAAR(args):
    baseDir  = getBaseDir()
    buildDir = getBuildDir('routing-android-aar')
    distDir  = getDistDir('routing-android')
    version  = args.buildversion
    distName = getRoutingAndroidAarDistName(version)

    if not gradle(args, '%s/scripts' % baseDir,
        '-p', 'routing-android',
        '--project-cache-dir', buildDir,
        '--gradle-user-home', '%s/gradle' % buildDir,
        '-Dbuild-version=%s' % version,
        'assembleRelease'
    ):
        return False

    # Gradle may place the AAR in one of several locations
    candidates = [
        '%s/outputs/aar/routing-android-release.aar'   % buildDir,
        '%s/outputs/aar/android-release.aar'           % buildDir,
        '%s/outputs/aar/valhalla-routing-release.aar'  % buildDir,
    ]
    aarFile = next((p for p in candidates if os.path.exists(p)), None)
    if aarFile is None:
        # Walk and find the first .aar under buildDir
        for root, _, files in os.walk(buildDir):
            for f in files:
                if f.endswith('.aar'):
                    aarFile = os.path.join(root, f)
                    break
            if aarFile:
                break
    if aarFile is None:
        print("ERROR: Could not find built AAR under %s" % buildDir)
        return False

    return makedirs(distDir) and copyfile(aarFile, '%s/%s' % (distDir, distName))


parser = argparse.ArgumentParser(description="Build Valhalla routing library for Android (.aar)")
parser.add_argument('--cmake',          dest='cmake',          default='cmake',    help='CMake executable')
parser.add_argument('--gradle',         dest='gradle',         default='gradle',   help='Gradle executable')
parser.add_argument('--configuration',  dest='configuration',  default='Release',
                    choices=['Release', 'RelWithDebInfo', 'Debug'])
parser.add_argument('--build-number',   dest='buildnumber',    default='',         help='Build sequence number')
parser.add_argument('--build-version',  dest='buildversion',   default='%s-devel' % SDK_VERSION,
                    help='Build version, embedded in the dist file name')
args = parser.parse_args()

if not checkExecutable(args.gradle, '--help'):
    print('Failed to find Gradle executable. Use --gradle to specify its location.')
    sys.exit(-1)

if not buildRoutingAAR(args):
    sys.exit(-1)

print("Done. Output: dist/routing-android/%s" % getRoutingAndroidAarDistName(args.buildversion))
