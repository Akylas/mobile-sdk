import re, sys, statistics
# Summarize PROF lines produced by ab.sh, grouped by [label].
# Windows longer than 1600 ms are idle periods and are discarded (see render-perf-method).
pat = re.compile(r"\[(?P<label>[^\]]+)\].*PROF: (?P<frames>\d+) frames in (?P<ms>\d+) ms \((?P<fps>[\d.]+) fps\), frame avg (?P<avg>[\d.]+) max [\d.]+ \| sky (?P<sky>[\d.]+) prelude (?P<prelude>[\d.]+) prepare (?P<prepare>[\d.]+) cover (?P<cover>[\d.]+) drape (?P<drape>[\d.]+) layers (?P<layers>[\d.]+) layers3D (?P<l3d>[\d.]+)")
groups = {}
for line in sys.stdin:
    m = pat.search(line)
    if not m:
        continue
    if float(m.group("ms")) > 1600:
        continue
    groups.setdefault(m.group("label"), []).append(m)
print(f"{'config':<28} {'n':>3} {'fps med':>8} {'frame':>7} {'sky':>6} {'prelude':>8} {'drape':>7} {'layers':>7}")
for label, ms in groups.items():
    f = lambda k: statistics.median(float(x.group(k)) for x in ms)
    print(f"{label:<28} {len(ms):>3} {f('fps'):>8.1f} {f('avg'):>7.1f} {f('sky'):>6.1f} {f('prelude'):>8.1f} {f('drape'):>7.1f} {f('layers'):>7.1f}")
