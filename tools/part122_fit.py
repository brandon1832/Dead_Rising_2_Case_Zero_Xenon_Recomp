#!/usr/bin/env python3
"""Per-arm LINEAR FIT of frame cost against draw count, read at one common load.

WHY THIS EXISTS. `tools/part116_guestcpu.py` compares arms in matched 250-draw bands and
takes the MEDIAN of each band's per-window frame time. On the crowd route two things make
that fragile, and part 122 hit both in its first campaign:

  * the route lands at a different spot every run (random zombie spawns), so the bands
    each arm populates differ and a 6-band "monotone" verdict can rest on n=1..6 windows
    a side;
  * the per-window MEDIAN wall is quantised to whole milliseconds by the 1 ms vblank
    (gotcha 572), so a band's wall moves in ~1 ms steps and a band median can read -1.07
    where the arm is worth -0.4.

The shadow-bias fix read "wall -1.07 ms, monotone" on the band reader; this fit, using
every window of all three runs an arm and the per-window MEAN frame time, read cz-draw
-0.56..-0.60 (three independent arms of the same code agreeing to 0.04) and the wall
-0.35..-0.5. The commit message was corrected before it was pushed.

The fit is y = a + b*draws per arm per column, over windows at >= MIN_DRAWS, evaluated
at one load; ± is the residual standard error of the mean. Read the CPU columns first
(cz-draw is the renderer thread, 'pump cpu' on the [fps] line); the wall is the noisiest.

Usage:
    tools/part122_fit.py <draws> tag="glob" [tag="glob" ...]
    tools/part122_fit.py 10300 old="$O/crowd_*_old?.log" new="$O/crowd_*_new?.log"
"""
import glob
import re
import statistics as st
import sys

MIN_DRAWS = 9000
PAT = re.compile(r"\[fps\][^(]*\(([\d.]+) ms\).*?draws med (\d+).*?pump cpu ([\d.]+) ms/frame"
                 r".*?guest main ([-\d.]+) draw ([-\d.]+)")


def load(pattern):
    rows = []
    for path in sorted(glob.glob(pattern)):
        for line in open(path, errors="replace"):
            m = PAT.search(line)
            if m and int(m.group(2)) >= MIN_DRAWS and float(m.group(4)) >= 0:
                rows.append(tuple(float(x) for x in m.groups()))
    return rows


def fit(rows, col, at):
    xs = [r[1] for r in rows]
    ys = [r[col] for r in rows]
    mx, my = st.mean(xs), st.mean(ys)
    sxx = sum((x - mx) ** 2 for x in xs)
    b = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sxx if sxx else 0.0
    res = [y - (my + b * (x - mx)) for x, y in zip(xs, ys)]
    return my + b * (at - mx), st.pstdev(res) / len(xs) ** 0.5


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    at = float(sys.argv[1])
    print(f"at {at:.0f} draws (wall = per-window MEAN frame ms; ± = se of the fit)")
    for arm in sys.argv[2:]:
        tag, pattern = arm.split("=", 1)
        rows = load(pattern)
        if len(rows) < 3:
            print(f"{tag:10s} only {len(rows)} windows at >= {MIN_DRAWS} draws — no fit")
            continue
        cols = []
        for col, name in ((0, "wall"), (2, "cz-draw"), (3, "main"), (4, "drawthr")):
            v, se = fit(rows, col, at)
            cols.append(f"{name} {v:6.2f}±{se:.2f}")
        lo, hi = min(r[1] for r in rows), max(r[1] for r in rows)
        print(f"{tag:10s} n={len(rows):3d}  " + "  ".join(cols) + f"   draws {lo:.0f}-{hi:.0f}")


if __name__ == "__main__":
    main()
