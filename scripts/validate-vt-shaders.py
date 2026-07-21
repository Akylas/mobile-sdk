#!/usr/bin/env python3
"""Compose vt shader program variants the same way GLTileRenderer::createShaderProgram
does and validate them with glslangValidator (ESSL 100)."""
import re, subprocess, sys, os, tempfile

SHADERS_H = "/Volumes/dev/carto/mobile-sdk/libs-carto/vt/src/vt/GLTileRendererShaders.h"
TILERENDERER_CPP = "/Volumes/dev/carto/mobile-sdk/all/native/renderers/TileRenderer.cpp"

def extract_strings(path):
    src = open(path).read()
    out = {}
    for m in re.finditer(r'(?:const std::string (?:TileRenderer::)?|static const std::string )(\w+)\s*=\s*R"GLSL\((.*?)\)GLSL"', src, re.S):
        out[m.group(1)] = m.group(2)
    return out

vt = extract_strings(SHADERS_H)
tr = extract_strings(TILERENDERER_CPP)

def compose(defs, *parts):
    s = "#version 100\n"
    for d in sorted(defs):
        s += "#define %s\n" % d
    return s + "".join(parts)

def validate(name, stage, source):
    suffix = ".vert" if stage == "vert" else ".frag"
    with tempfile.NamedTemporaryFile("w", suffix=suffix, delete=False) as f:
        f.write(source)
        path = f.name
    r = subprocess.run(["glslangValidator", path], capture_output=True, text=True)
    os.unlink(path)
    if r.returncode != 0:
        print("FAIL %s (%s)" % (name, stage))
        print(r.stdout.strip())
        print("----- source -----")
        for i, line in enumerate(source.split("\n"), 1):
            print("%3d %s" % (i, line))
        return False
    return True

L2D_VSH = tr["LIGHTING_SHADER_2D"]        # per-vertex -> LIGHTING_VSH, goes into vsh
L3D_VSH = tr["LIGHTING_SHADER_3D"]        # per-vertex -> LIGHTING_VSH
LNM_FSH = tr["LIGHTING_SHADER_NORMALMAP"] # per-fragment -> LIGHTING_FSH, goes into fsh
FILT = vt["textureFiltersFsh"]

ok = True
def check(name, defs, vsh_parts, fsh_parts):
    global ok
    v = compose(defs, vt["commonVsh"], *vsh_parts)
    f = compose(defs, vt["commonFsh"], *fsh_parts)
    ok = validate(name, "vert", v) and ok
    ok = validate(name, "frag", f) and ok

T = "TERRAIN_DEPTH_BIAS"
V = "TERRAIN"  # GPU draping: vertex-shader elevation texture displacement
for terrain in ([], [T], [T, V]):
    tag = ("+terrainvtf" if V in terrain else ("+terrain" if terrain else ""))
    # tilebackground: GEOMETRY2D lighting (per-vertex in the SDK)
    for pat in ([], ["PATTERN"]):
        defs = set(terrain) | set(pat) | {"LIGHTING_VSH"}
        check("tilebackground%s%s" % (tag, "+pat" if pat else ""), defs,
              [L2D_VSH, vt["backgroundVsh"]], [vt["backgroundFsh"]])
    # tilecolormap: PATTERN always set, filter modes
    for filt in ("FILTER_NEAREST", "FILTER_BILINEAR", "FILTER_BICUBIC"):
        defs = set(terrain) | {"PATTERN", filt, "LIGHTING_VSH"}
        check("tilecolormap%s+%s" % (tag, filt), defs,
              [L2D_VSH, vt["colormapVsh"]], [FILT, vt["colormapFsh"]])
    # tilenormalmap: NORMALMAP lighting (per-fragment in the SDK)
    defs = set(terrain) | {"PATTERN", "FILTER_BILINEAR", "LIGHTING_FSH"}
    check("tilenormalmap%s" % tag, defs,
          [vt["normalmapVsh"]], [LNM_FSH, FILT, vt["normalmapFsh"]])
    # point/line/polygon with full flag combos
    for flags in ([], ["PATTERN"], ["TRANSFORM"], ["OFFSET"], ["PATTERN", "TRANSFORM", "OFFSET"]):
        defs = set(terrain) | set(flags) | {"LIGHTING_VSH"}
        check("point%s+%s" % (tag, "".join(flags) or "base"), defs,
              [L2D_VSH, vt["pointVsh"]], [vt["pointFsh"]])
        check("line%s+%s" % (tag, "".join(flags) or "base"), defs | {"DERIVATIVES"},
              [L2D_VSH, vt["lineVsh"]], [vt["lineFsh"]])
        pdefs = set(terrain) | (set(flags) - {"OFFSET"}) | {"LIGHTING_VSH"}
        check("polygon%s+%s" % (tag, "".join(flags) or "base"), pdefs,
              [L2D_VSH, vt["polygonVsh"]], [vt["polygonFsh"]])

# tilemask with GPU draping (TERRAIN only, no depth bias)
check("tilemask+terrainvtf", {V}, [vt["backgroundVsh"]], [vt["backgroundFsh"]])

# unchanged shaders, baseline sanity
check("polygon3d", {"LIGHTING_VSH"}, [L3D_VSH, vt["polygon3DVsh"]], [vt["polygon3DFsh"]])
check("polygon3d+terrainvtf", {"LIGHTING_VSH", V}, [L3D_VSH, vt["polygon3DVsh"]], [vt["polygon3DFsh"]])
check("label", {"LIGHTING_VSH"}, [L2D_VSH, vt["labelVsh"]], [vt["labelFsh"]])
check("blend", set(), [vt["blendVsh"]], [vt["blendFsh"]])

print("ALL OK" if ok else "FAILURES FOUND")
sys.exit(0 if ok else 1)
