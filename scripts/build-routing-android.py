#!/usr/bin/env python3
"""
Build script for the standalone routing library (Android).

Usage:
    python3 scripts/build-routing-android.py \
        --androidndkpath /path/to/ndk \
        [--configuration Release|Debug] \
        [--abi arm64-v8a|armeabi-v7a|x86_64|x86] \
        [--with-zstd] \
        [--shared]

Output:
    dist/routing-android/<abi>/libcarto_routing.so  (shared)
    dist/routing-android/<abi>/libcarto_routing.a   (static)
"""

import os
import sys
import shutil
import argparse
import subprocess

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def repo_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def makedirs(path):
    os.makedirs(path, exist_ok=True)

def run_cmake(cmake_exe, cwd, args):
    cmd = [cmake_exe] + args
    print(">> " + " ".join(cmd))
    result = subprocess.run(cmd, cwd=cwd)
    if result.returncode != 0:
        print("CMake command failed with exit code %d" % result.returncode)
        sys.exit(result.returncode)

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

ANDROID_ABIS    = ["arm64-v8a", "armeabi-v7a", "x86_64"]
ANDROID_API_64  = 21
ANDROID_API_32  = 19

def build(args):
    base_dir  = repo_root()
    cmake_src = os.path.join(base_dir, "scripts", "build", "routing-CMakeLists.txt")
    dist_base = os.path.join(base_dir, "dist", "routing-android")

    abis = args.androidabi if args.androidabi else ANDROID_ABIS
    lib_name = "libcarto_routing.%s" % ("so" if args.shared else "a")

    for abi in abis:
        api = ANDROID_API_64 if "64" in abi else ANDROID_API_32

        build_dir = os.path.join(base_dir, "build", "routing-android", abi)
        dist_dir  = os.path.join(dist_base, abi)
        makedirs(build_dir)
        makedirs(dist_dir)

        cmake_defines = [
            "-DCMAKE_SYSTEM_NAME=Android",
            "-DCMAKE_BUILD_TYPE=%s"          % args.configuration,
            "-DCMAKE_ANDROID_NDK=%s"         % args.androidndkpath,
            "-DCMAKE_ANDROID_ARCH_ABI=%s"    % abi,
            "-DANDROID_STL=c++_static",
            "-DANDROID_NDK=%s"               % args.androidndkpath,
            "-DANDROID_ABI=%s"               % abi,
            "-DSINGLE_LIBRARY:BOOL=ON",
            "-DSDK_CPP_DEFINES=%s" % " ".join(defines),
            "-DSDK_PLATFORM='Android'",
            "-DSDK_ANDROID_ABI='%s'" % abi,
            "-DSDK_VERSION='%s'" % version,
            "-DANDROID_PLATFORM=android-%d"  % api,
            "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON",
            "-DROUTING_WITH_ZSTD:BOOL=%s"    % ("ON" if args.with_zstd else "OFF"),
            "-DROUTING_SHARED:BOOL=%s"       % ("ON" if args.shared else "OFF"),
            "-DROUTING_WITH_VALHALLA:BOOL=ON",
        ]

        if args.make:
            cmake_defines += ["-DCMAKE_MAKE_PROGRAM=%s" % args.make]

        # Generate
        run_cmake(args.cmake, build_dir, cmake_defines + [cmake_src])

        # Build
        run_cmake(args.cmake, build_dir, [
            "--build", ".",
            "--config", args.configuration,
            "--parallel", str(os.cpu_count() or 4),
        ])

        # Copy output
        built_lib = os.path.join(build_dir, lib_name)
        if os.path.exists(built_lib):
            shutil.copy2(built_lib, os.path.join(dist_dir, lib_name))
            print("Copied %s -> %s/%s" % (lib_name, dist_dir, lib_name))
        else:
            print("WARNING: Could not find built library at %s" % built_lib)

    print("\nBuild complete. Output in: %s" % dist_base)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Build routing library for Android")
    parser.add_argument("--cmake",           default="cmake",  help="Path to cmake executable")
    parser.add_argument("--make",            default=None,     help="Path to make/ninja executable")
    parser.add_argument('--android-ndk-path', dest='androidndkpath', default='auto', help='Android NDK path')
    parser.add_argument("--configuration",   default="Release", choices=["Release", "Debug"])
    parser.add_argument('--android-abi', dest='androidabi', default=[], choices=ANDROID_ABIS + ['all'], action='append', help='Android target ABIs')
    parser.add_argument('--defines', dest='defines', default='', help='Defines for compilation')
    parser.add_argument('--gradle', dest='gradle', default='gradle', help='Gradle executable')
    parser.add_argument("--with-zstd",       action="store_true", default=True)
    parser.add_argument('--build-number', dest='buildnumber', default='', help='Build sequence number, goes to version str')
    parser.add_argument('--build-version', dest='buildversion', default='%s-devel' % SDK_VERSION, help='Build version, goes to distributions')
    parser.add_argument("--shared",          action="store_true", default=False)
    args = parser.parse_args()

if 'all' in args.androidabi or args.androidabi == []:
  args.androidabi = ANDROID_ABIS
if args.androidsdkpath == 'auto':
  args.androidsdkpath = os.environ.get('ANDROID_HOME', None)
  if args.androidsdkpath is None:
    print("ANDROID_HOME variable not set")
    sys.exit(-1)
if args.androidndkpath == 'auto':
  args.androidndkpath = os.environ.get('ANDROID_NDK_HOME', None)
  if args.androidndkpath is None:
    args.androidndkpath = os.path.join(args.androidsdkpath, 'ndk-bundle')
args.defines += ';' + getProfile(args.profile).get('defines', '')
build(args)
