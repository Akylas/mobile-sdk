import os
import string
import sys
from build.sdk_build_utils import *

def generate_jitpack_yml(version, profiles, include_routing=True):
    baseDir = getBaseDir()
    template_path = os.path.join(baseDir, 'scripts', 'android-jitpack', 'jitpack.yml.template')
    distDir = os.path.join(baseDir, 'dist', 'android')
    output_path = os.path.join(distDir, 'jitpack.yml')
    makedirs(distDir)

    if not os.path.exists(template_path):
        print(f"Template file not found: {template_path}")
        sys.exit(1)

    with open(template_path, 'r') as template_file:
        template = string.Template(template_file.read())

    profiles = [p.strip() for p in (profiles or "").split(',') if p.strip()]
    routing_aar_url = (
        "%s/releases/download/v%s/%s" % (REPO_URL, version, getRoutingAndroidAarDistName(version))
        if include_routing else ""
    )
    jitpack_content = template.safe_substitute({
        'version':       version,
        'variants':      ','.join(getVariant(p) for p in profiles),
        'aarUrls':       ','.join("%s/releases/download/v%s/%s" % (REPO_URL, version, getAndroidAarDistName(version, p)) for p in profiles),
        'repo_url':      REPO_URL,
        'routingAarUrl': routing_aar_url,
    })

    with open(output_path, 'w') as output_file:
        output_file.write(jitpack_content)

    print(f"Generated jitpack.yml at: {output_path}")

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--version',         required=True,  dest='version')
    parser.add_argument('--profiles',        required=True,  dest='profiles')
    parser.add_argument('--no-routing',      dest='include_routing', action='store_false', default=True,
                        help='Omit routing AAR from jitpack.yml')
    args = parser.parse_args()

    generate_jitpack_yml(args.version, args.profiles, include_routing=args.include_routing)
