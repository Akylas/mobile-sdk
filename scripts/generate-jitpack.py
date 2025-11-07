import os
import string
import sys
from build.sdk_build_utils import *

def generate_jitpack_yml(version, profiles):
    baseDir = getBaseDir()
    template_path = os.path.join(baseDir, 'scripts', 'android-jitpack', 'jitpack.yml.template')
    output_path = os.path.join(baseDir, 'dist', 'android', 'jitpack.yml')

    if not os.path.exists(template_path):
        print(f"Template file not found: {template_path}")
        sys.exit(1)

    with open(template_path, 'r') as template_file:
        template = string.Template(template_file.read())
# $repoUrl/releases/download/$version/$distName

    profiles = [p.strip() for p in (profiles or "").split(',') if p.strip()]
    jitpack_content = template.safe_substitute({
        'version': version,
        'variants': ','.join(getVariant(p) for p in profiles),
        'aarUrls': ','.join("%s/releases/download/%s/%s" % (REPO_URL, version, getAndroidAarDistName(version, p)) for p in profiles),
        'repo_url': REPO_URL
    })

    with open(output_path, 'w') as output_file:
        output_file.write(jitpack_content)

    print(f"Generated jitpack.yml at: {output_path}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: generate-jitpack.py <version> <profile>")
        sys.exit(1)

    version = sys.argv[1]
    profiles = sys.argv[2]

    generate_jitpack_yml(version, profiles)