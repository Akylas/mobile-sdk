import re, sys, statistics
# Median summary of the 2D city runs: PROF (CPU) and PROF GPU lines, grouped by [label].
# Windows longer than 1600 ms are idle periods and are discarded (see render-perf-method).
cpu = re.compile(r"\[(?P<label>[^\]]+)\].*PROF: (?P<frames>\d+) frames in (?P<ms>\d+) ms \((?P<fps>[\d.]+) fps\), frame avg (?P<avg>[\d.]+) max [\d.]+ \| sky (?P<sky>[\d.]+) prelude [\d.]+ prepare [\d.]+ cover [\d.]+ drape [\d.]+ layers (?P<layers>[\d.]+) layers3D (?P<l3d>[\d.]+)")
gpu = re.compile(r"\[(?P<label>[^\]]+)\].*PROF GPU: \d+ frames.*\| sky (?P<sky>[\d.]+) background (?P<bg>[\d.]+) .*layers (?P<layers>[\d.]+) layers3D (?P<l3d>[\d.]+) .*total (?P<total>[\d.]+)")
c, g = {}, {}
for line in sys.stdin:
    m = cpu.search(line)
    if m and float(m.group("ms")) <= 1600:
        c.setdefault(m.group("label"), []).append(m)
    m = gpu.search(line)
    if m:
        g.setdefault(m.group("label"), []).append(m)
med = lambda ms, k: statistics.median(float(x.group(k)) for x in ms)
print(f"{'config':<22} {'n':>3} {'fps':>6} {'cpuFrm':>7} {'swait':>6} {'layers':>7} {'lyr3D':>6} | {'gpuTot':>7} {'gSky':>5} {'gBg':>5} {'gLyr':>6}")
for label in c:
    m, gm = c[label], g.get(label, [])
    row = f"{label:<22} {len(m):>3} {med(m,'fps'):>6.1f} {med(m,'avg'):>7.1f} {med(m,'sky'):>6.1f} {med(m,'layers'):>7.1f} {med(m,'l3d'):>6.1f}"
    row += f" | {med(gm,'total'):>7.1f} {med(gm,'sky'):>5.1f} {med(gm,'bg'):>5.1f} {med(gm,'layers'):>6.1f}" if gm else " |"
    print(row)
