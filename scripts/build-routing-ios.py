#!/usr/bin/env python3
"""
Build script for the standalone routing library (iOS / macOS / Mac Catalyst).

Usage:
    python3 scripts/build-routing-ios.py \
        [--configuration Release|Debug] \
        [--arch arm64|x86_64|...] \
        [--with-zstd] \
        [--shared]

Output:
    dist/routing-ios/<arch>/libcarto_routing.a  (or .dylib)
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

IOS_ARCHS = ["arm64", "x86_64"]

def build(args):
    base_dir  = repo_root()
    cmake_src = os.path.join(base_dir, "scripts", "build", "routing-CMakeLists.txt")
    dist_base = os.path.join(base_dir, "dist", "routing-ios")

    archs = args.arch if args.arch else IOS_ARCHS
    lib_name = "libcarto_routing.%s" % ("dylib" if args.shared else "a")

    for arch in archs:
        # Determine platform
        if arch in ("x86_64", "i386", "arm64-simulator"):
            platform = "SIMULATOR"
            base_arch = arch.replace("-simulator", "")
        elif arch.endswith("-maccatalyst"):
            platform = "MACCATALYST"
            base_arch = arch[:-12]
        else:
            platform = "OS"
            base_arch = arch

        build_dir = os.path.join(base_dir, "build", "routing-ios", arch)
        dist_dir  = os.path.join(dist_base, arch)
        makedirs(build_dir)
        makedirs(dist_dir)

        cmake_defines = [
            "-DCMAKE_TOOLCHAIN_FILE=%s/scripts/build/ios-toolchain.cmake" % base_dir,
            "-DPLATFORM=%s"          % platform,
            "-DARCHS=%s"             % base_arch,
            "-DCMAKE_BUILD_TYPE=%s"  % args.configuration,
            "-DROUTING_WITH_ZSTD:BOOL=%s"   % ("ON" if args.with_zstd else "OFF"),
            "-DROUTING_SHARED:BOOL=%s"       % ("ON" if args.shared else "OFF"),
            "-DROUTING_WITH_VALHALLA:BOOL=ON",
            "-G", "Xcode",
        ]

        # Generate
        run_cmake(args.cmake, build_dir, cmake_defines + [cmake_src])

        # Build
        run_cmake(args.cmake, build_dir, [
            "--build", ".",
            "--config", args.configuration,
            "--parallel", str(os.cpu_count() or 4),
        ])

        # Copy output
        built_lib = os.path.join(build_dir, args.configuration, lib_name)
        if not os.path.exists(built_lib):
            # Some CMake generators put it in a sub-folder
            built_lib = os.path.join(build_dir, lib_name)

        if os.path.exists(built_lib):
            shutil.copy2(built_lib, os.path.join(dist_dir, lib_name))
            print("Copied %s -> %s/%s" % (lib_name, dist_dir, lib_name))
        else:
            print("WARNING: Could not find built library at %s" % built_lib)

    print("\nBuild complete. Output in: %s" % dist_base)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Build routing library for iOS")
    parser.add_argument("--cmake",         default="cmake",  help="Path to cmake executable")
    parser.add_argument("--configuration", default="Release", choices=["Release", "Debug"])
    parser.add_argument("--arch",          action="append",  help="Target architecture(s)")
    parser.add_argument("--with-zstd",     action="store_true", default=True)
    parser.add_argument("--shared",        action="store_true", default=False)
    args = parser.parse_args()
    build(args)
