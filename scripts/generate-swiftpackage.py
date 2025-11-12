import os
import string
import sys
import hashlib
import urllib.request
import json
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

def generate_swift_package(version, profiles_csv, checksums_map=None):
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

    entries = []
    for p in profiles:
        variant = getVariant(p)
        target_name = f"{framework_base}-{variant}"
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
        entries.append(entry)

    targets_array = "[\n" + "".join(entries) + "    ]"

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
    parser.add_argument('--version', required=True, dest='version')
    parser.add_argument('--profiles', required=True, dest='profiles')
    parser.add_argument('--checksums-file', dest='checksums_file', default=None)
    args = parser.parse_args()

    checksums_map = None
    if args.checksums_file:
        if not os.path.exists(args.checksums_file):
            print("Checksums file not found:", args.checksums_file)
            sys.exit(1)
        with open(args.checksums_file, 'r') as f:
            checksums_map = json.load(f)

    generate_swift_package(args.version, args.profiles, checksums_map)