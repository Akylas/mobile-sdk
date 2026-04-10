import os
import sys
import shutil
import argparse
from build.sdk_build_utils import *

IOS_ARCHS = ['arm64', 'arm64-simulator', 'x86_64']

FRAMEWORK_NAME = "ValhallaRouting"

def getPlatformArch(baseArch):
    if baseArch.endswith('-maccatalyst'):
        return 'MACCATALYST', baseArch[:-12]
    if baseArch.endswith('-simulator'):
        return 'SIMULATOR', baseArch[:-10]
    return ('OS' if baseArch.startswith('arm') else 'SIMULATOR'), baseArch

def buildRoutingLib(args, baseArch):
    platform, arch = getPlatformArch(baseArch)
    version  = getVersion(args.buildversion, args.buildnumber) if args.configuration == 'Release' else 'Devel'
    baseDir  = getBaseDir()
    buildDir = getBuildDir('routing-ios', '%s-%s' % (platform, arch))

    sysroot = {
        'OS':          'iphoneos',
        'SIMULATOR':   'iphonesimulator',
        'MACCATALYST': 'macosx',
    }[platform]
    deployTarget = '13.0' if platform == 'MACCATALYST' else '12.0'

    if not cmake(args, buildDir, [
        '-G', 'Xcode',
        '-DCMAKE_SYSTEM_NAME=%s'             % ('Darwin' if platform == 'MACCATALYST' else 'iOS'),
        '-DCMAKE_OSX_ARCHITECTURES=%s'        % arch,
        '-DCMAKE_OSX_SYSROOT=%s'              % sysroot,
        '-DCMAKE_OSX_DEPLOYMENT_TARGET=%s'    % deployTarget,
        '-DCMAKE_BUILD_TYPE=%s'               % args.configuration,
        '-DSDK_VERSION=%s'                    % version,
        '%s/scripts/routing' % baseDir
    ]):
        return False

    return cmake(args, buildDir, [
        '--build', '.',
        '--config', args.configuration,
        '--parallel', str(os.cpu_count() or 4),
    ])

def buildRoutingXCFramework(args, baseArchs):
    baseDir  = getBaseDir()
    distDir  = getDistDir('routing-ios')
    version  = args.buildversion
    distName = getRoutingIOSZipDistName(version)

    # Copy the public header into a staging area
    headersDir  = getBuildDir('routing-ios', 'Headers')
    headersDest = '%s/%s' % (headersDir, FRAMEWORK_NAME)
    makedirs(headersDest)
    if not copyfile('%s/routing-lib/ios/NTValhallaRoutingService.h' % baseDir,
                    '%s/NTValhallaRoutingService.h' % headersDest):
        return False

    # Group by platform and lipo-combine architectures
    groupedPlatformArchs = {}
    for baseArch in baseArchs:
        platform, _ = getPlatformArch(baseArch)
        groupedPlatformArchs.setdefault(platform, []).append(baseArch)

    frameworkOptions = []
    for platform, pArchs in groupedPlatformArchs.items():
        libFiles = []
        for baseArch in pArchs:
            plat, arch = getPlatformArch(baseArch)
            buildDir = getBuildDir('routing-ios', '%s-%s' % (plat, arch))
            # Xcode places the product in <buildDir>/<config>-iphone{os,simulator}/
            candidates = [
                '%s/%s-%s/libValhallaRouting.a'  % (buildDir, args.configuration, ('iphoneos'        if plat == 'OS'       else 'iphonesimulator')),
                '%s/%s/libValhallaRouting.a'      % (buildDir, args.configuration),
                '%s/libValhallaRouting.a'          % buildDir,
            ]
            libFile = next((p for p in candidates if os.path.exists(p)), None)
            if libFile is None:
                # Walk and find it
                for root, _, files in os.walk(buildDir):
                    for f in files:
                        if f == 'libValhallaRouting.a':
                            libFile = os.path.join(root, f)
                            break
                    if libFile:
                        break
            if libFile is None:
                print("ERROR: Could not find libValhallaRouting.a for %s" % baseArch)
                return False
            libFiles.append(libFile)

        fatLib = '%s/libValhallaRouting.a' % getBuildDir('routing-ios', platform)
        if len(libFiles) > 1:
            if not execute('lipo', baseDir, '-output', fatLib, '-create', *libFiles):
                return False
        else:
            if not copyfile(libFiles[0], fatLib):
                return False
        frameworkOptions.extend(['-library', fatLib, '-headers', headersDir])

    xcframeworkPath = '%s/%s.xcframework' % (distDir, FRAMEWORK_NAME)
    shutil.rmtree(xcframeworkPath, True)
    if not execute('xcodebuild', baseDir,
        '-create-xcframework', '-output', xcframeworkPath,
        *frameworkOptions
    ):
        return False

    # Zip the XCFramework
    zipPath = '%s/%s' % (distDir, distName)
    try:
        os.remove(zipPath)
    except:
        pass
    if not execute('ditto', distDir,
        '-c', '-k', '--sequesterRsrc', '--keepParent',
        '%s.xcframework' % FRAMEWORK_NAME,
        zipPath
    ):
        return False

    print("iOS XCFramework output available in:\n%s" % distDir)
    return True


parser = argparse.ArgumentParser(description="Build Valhalla routing library for iOS (XCFramework)")
parser.add_argument('--cmake',           dest='cmake',          default='cmake', help='CMake executable')
parser.add_argument('--ios-arch',        dest='iosarch',        default=[],
                    choices=IOS_ARCHS + ['all'], action='append', help='iOS target architectures')
parser.add_argument('--configuration',   dest='configuration',  default='Release',
                    choices=['Release', 'RelWithDebInfo', 'Debug'])
parser.add_argument('--build-number',    dest='buildnumber',    default='', help='Build sequence number')
parser.add_argument('--build-version',   dest='buildversion',   default='%s-devel' % SDK_VERSION,
                    help='Build version, embedded in the dist file name')
parser.add_argument('--build-xcframework', dest='buildxcframework', default=True, action='store_true')
args = parser.parse_args()

if 'all' in args.iosarch or args.iosarch == []:
    args.iosarch = IOS_ARCHS

if not checkExecutable(args.cmake, '--help'):
    print('Failed to find CMake executable. Use --cmake to specify its location.')
    sys.exit(-1)

for arch in args.iosarch:
    if not buildRoutingLib(args, arch):
        sys.exit(-1)

if args.buildxcframework:
    if not buildRoutingXCFramework(args, args.iosarch):
        sys.exit(-1)

print("Done. Output: dist/routing-ios/%s" % getRoutingIOSZipDistName(args.buildversion))
