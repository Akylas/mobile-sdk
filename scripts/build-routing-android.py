import os
import sys
import shutil
import argparse
from build.sdk_build_utils import *

ANDROID_ABIS = ['armeabi-v7a', 'x86', 'arm64-v8a', 'x86_64']

def gradle(args, dir, *cmdArgs):
    return execute(args.gradle, dir, *cmdArgs)

def buildRoutingAAR(args):
    baseDir  = getBaseDir()
    buildDir = getBuildDir('routing-android-aar')
    distDir  = getDistDir('routing-android')
    version  = args.buildversion
    distName = getRoutingAndroidAarDistName(version)

    defines = ["-D%s" % define for define in args.defines.split(';') if define]
    options = ["-D%s" % option for option in args.cmakeoptions.split(';') if option]

    # Pass NDK/SDK paths and ABI filter as Gradle project properties so that the
    # routing-android build.gradle can pick them up.
    gradleArgs = [
        '-p', 'routing-android',
        '--project-cache-dir', buildDir,
        '--gradle-user-home', '%s/gradle' % buildDir,
        '-Dbuild-version=%s' % version,
    ]
    if args.androidndkpath and args.androidndkpath != 'auto':
        gradleArgs += ['-PandroidNdkPath=%s' % args.androidndkpath]
    if args.androidsdkpath and args.androidsdkpath != 'auto':
        gradleArgs += ['-PandroidSdkPath=%s' % args.androidsdkpath]
    if args.androidabi:
        gradleArgs += ['-PandroidAbi=%s' % ','.join(args.androidabi)]
    if defines:
        gradleArgs += ['-PsdkDefines=%s' % ' '.join(defines)]
    if options:
        gradleArgs += ['-PcmakeOptions=%s' % ';'.join(options)]
    if args.cmake and args.cmake != 'cmake':
        gradleArgs += ['-PcmakePath=%s' % args.cmake]

    gradleArgs += ['assembleRelease']

    if not gradle(args, '%s/scripts' % baseDir, *gradleArgs):
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
parser.add_argument('--cmake',              dest='cmake',          default='cmake',    help='CMake executable')
parser.add_argument('--cmake-options',      dest='cmakeoptions',   default='',         help='CMake options')
parser.add_argument('--make',               dest='make',           default='make',     help='Make executable')
parser.add_argument('--gradle',             dest='gradle',         default='gradle',   help='Gradle executable')
parser.add_argument('--android-ndk-path',   dest='androidndkpath', default='auto',     help='Android NDK path')
parser.add_argument('--android-sdk-path',   dest='androidsdkpath', default='auto',     help='Android SDK path')
parser.add_argument('--android-abi',        dest='androidabi',     default=[],
                    choices=ANDROID_ABIS + ['all'], action='append',
                    help='Android target ABI(s). Can be specified multiple times.')
parser.add_argument('--defines',            dest='defines',        default='',         help='C++ preprocessor defines (semicolon-separated)')
parser.add_argument('--configuration',      dest='configuration',  default='Release',
                    choices=['Release', 'RelWithDebInfo', 'Debug'])
parser.add_argument('--build-number',       dest='buildnumber',    default='',         help='Build sequence number')
parser.add_argument('--build-version',      dest='buildversion',   default='%s-devel' % SDK_VERSION,
                    help='Build version, embedded in the dist file name')
args = parser.parse_args()

if 'all' in args.androidabi or args.androidabi == []:
    args.androidabi = ANDROID_ABIS

if args.androidsdkpath == 'auto':
    args.androidsdkpath = os.environ.get('ANDROID_HOME', None)
if args.androidndkpath == 'auto':
    args.androidndkpath = os.environ.get('ANDROID_NDK_HOME', None)
    if args.androidndkpath is None and args.androidsdkpath:
        args.androidndkpath = os.path.join(args.androidsdkpath, 'ndk-bundle')

if not checkExecutable(args.gradle, '--help'):
    print('Failed to find Gradle executable. Use --gradle to specify its location.')
    sys.exit(-1)

if not buildRoutingAAR(args):
    sys.exit(-1)

print("Done. Output: dist/routing-android/%s" % getRoutingAndroidAarDistName(args.buildversion))
