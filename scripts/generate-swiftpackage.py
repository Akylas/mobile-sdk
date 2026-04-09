import os
import string
import sys
import hashlib
import urllib.request
import json
import re
from build.sdk_build_utils import *

def checksum_url_sha256(url):
    try:
        with urllib.request.urlopen(url) as resp:
            sha256 = hashlib.sha256()
            while True:
                chunk = resp.read(8192)
                if not chunk:
                    break
                sha256.update(chunk)
            return sha256.hexdigest()
    except Exception as e:
        print(f"Failed to download/checksum {url}: {e}")
        return None

def generate_swift_package(version, profiles_csv, checksums_map=None, routing_checksum=None):
    baseDir = getBaseDir()
    template_path = os.path.join(baseDir, 'scripts', 'ios-swiftpackage', 'Package.swift.template')
    distDir = os.path.join(baseDir, 'dist', 'ios_metal')
    output_path = os.path.join(distDir, 'Package.swift')
    makedirs(distDir)

    if not os.path.exists(template_path):
        print(f"Template file not found: {template_path}")
        sys.exit(1)

    with open(template_path, 'r') as template_file:
        template_text = template_file.read()

    profiles = [p.strip() for p in (profiles_csv or "").split(',') if p.strip()]
    if not profiles:
        print("No profiles provided")
        sys.exit(1)

    framework_base = "CartoMobileSDK"

    def make_library_name(name):
        # Split on any non-alphanumeric separator and join by capitalizing subsequent parts
        parts = re.split(r'[^0-9A-Za-z]+', name)
        if not parts:
            return re.sub(r'[^0-9A-Za-z]', '', name)
        first = parts[0]
        rest = ''.join(p[:1].upper() + p[1:] for p in parts[1:] if p)
        return first + rest

    target_entries = []
    library_entries = []
    for idx, p in enumerate(profiles):
        variant = getVariant(p)
        # First profile uses base framework name, rest append variant
        target_name = framework_base if idx == 0 else f"{framework_base}-{variant}"
        dist_name = getIOSZipDistName(version, p)
        url = "%s/releases/download/v%s/%s" % (REPO_URL, version, dist_name)
        checksum = None
        if checksums_map:
            checksum = checksums_map.get(p) or checksums_map.get(variant)
        if not checksum:
            checksum = checksum_url_sha256(url)
        if checksum is None:
            print("Aborting due to checksum error for", p)
            sys.exit(1)
        entry = (
            "        .binaryTarget(\n"
            f'            name: "{target_name}",\n'
            f'            url: "{url}",\n'
            f'            checksum: "{checksum}"\n'
            "        ),\n"
        )
        target_entries.append(entry)
        lib_name = make_library_name(target_name)
        entry = (
            f'        .library(name: "{lib_name}", targets: ["{target_name}"]),\n'
        )
        library_entries.append(entry)

    # Add ValhallaRouting binary target
    routing_target_name = "ValhallaRouting"
    routing_dist_name = getRoutingIOSZipDistName(version)
    routing_url = "%s/releases/download/v%s/%s" % (REPO_URL, version, routing_dist_name)
    if not routing_checksum:
        routing_checksum = checksum_url_sha256(routing_url)
    if routing_checksum:
        routing_entry = (
            "        .binaryTarget(\n"
            f'            name: "{routing_target_name}",\n'
            f'            url: "{routing_url}",\n'
            f'            checksum: "{routing_checksum}"\n'
            "        ),\n"
        )
        target_entries.append(routing_entry)
        library_entries.append(
            f'        .library(name: "{routing_target_name}", targets: ["{routing_target_name}"]),\n'
        )
    else:
        print(f"Warning: could not compute checksum for {routing_url}, skipping ValhallaRouting target")

    targets_array = "[\n" + "".join(target_entries) + "    ]"
    libraries_array = "[\n" + "".join(library_entries) + "    ]"

    if '"$products"' in template_text:
        template_text = template_text.replace('"$products"', libraries_array)
        template = string.Template(template_text)

    if '"$targets"' in template_text:
        template_text = template_text.replace('"$targets"', targets_array)
        template = string.Template(template_text)
        content = template.safe_substitute({'frameworkName': framework_base})
    else:
        template = string.Template(template_text)
        content = template.safe_substitute({
            'frameworkName': framework_base,
            'targets': targets_array
        })

    with open(output_path, 'w') as f:
        f.write(content)

    print(f"Generated Package.swift at: {output_path}")

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--version',          required=True, dest='version')
    parser.add_argument('--profiles',         required=True, dest='profiles')
    parser.add_argument('--checksums-file',   dest='checksums_file',    default=None)
    parser.add_argument('--routing-checksum', dest='routing_checksum',  default=None,
                        help='SHA-256 checksum of the ValhallaRouting XCFramework zip')
    args = parser.parse_args()

    checksums_map = None
    if args.checksums_file:
        if not os.path.exists(args.checksums_file):
            print("Checksums file not found:", args.checksums_file)
            sys.exit(1)
        with open(args.checksums_file, 'r') as f:
            checksums_map = json.load(f)

    generate_swift_package(args.version, args.profiles, checksums_map, routing_checksum=args.routing_checksum)