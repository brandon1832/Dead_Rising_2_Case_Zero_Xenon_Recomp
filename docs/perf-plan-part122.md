# Part 122 — re-baseline after the detail defaults, and what is on `cz-draw` now

The operator's ask (2026-10-10 evening): *"Let's do performance improvement. Let's try to
find what to improve gpu or cpu wise what we can separate to other core and stuff like
that."* Part 118 was the last performance part; since then WORLD DETAIL and ZOMBIE DETAIL
shipped at 2.0, the hair fix, the shadow bias and the macOS work. So step 0 was the rule
every plan since part 71 opens with: **re-baseline before pricing anything.**

All runs: `tools/part80_crowdroute.sh` at 1920x1080, 70 s soak, the shipped defaults
(pin mode 2, two-core pump), the operator's persisted settings (world LOD 2.0). Campaigns:
`tools/part122_campaign.sh` (two binaries) and `tools/part122_campaign_env.sh` (env arms
on one binary), three runs an arm alternated, the CPU clock printed before every run.
Logs in `~/DR2CZ-troubleshooting/part122/`. **Read with `tools/part122_fit.py`** — see §5.

## §0. Where the frame is now

| term | part 118 close (9,000 draws) | part 122 start (10,300 draws) |
|---|---|---|
| draws at the crowd soak | 8,900-9,700 | **10,700-11,400** (the 2.0 detail defaults) |
| wall | ~9-10 ms | **12.2 ms** (mean) |
| `cz-draw` (the renderer thread) | 8.4-8.8 | **11.6** — the bound, by 3 ms |
| guest Main Thread | 7.9-8.0 | 8.4 |
| guest Draw Thread | 5.7-5.9 | 6.4 |
| GPU | — | 3.8 ms mean, fence wait 0.02 — not the bound |

`cz-draw` is 96% of a core in USER time (19.15 of 20 s, sys 0.28) — not the kernel.
Its idle (`[split] didle`) is ~1.0-1.3 ms/frame, almost all "after store" (§4).

## §1. The shadow bias read write-combined memory — FIXED, `3e6897b`

`perf` (user-only, `-p`, 20 s of the soak) on `cz-draw`: DoDraw 19%, UploadStream 13%,
the split's own loop 9% (replay + spin), texture lookup ~11%, **`ApplyShadowBias` 5.4%**,
the const-window copy 7%, the driver 8%. `ApplyShadowBias` is three multiplies; its cost
was `c[47*4+i] *= f` on the per-frame ARENA — HOST_VISIBLE|HOST_COHERENT, not
HOST_CACHED, so each read is an uncached DRAM round trip. Gotchas 446-447's defect,
reintroduced one function over by b6bf4fe the day before. Fixed by scaling the cached
source register and writing only. **`cz-draw` −0.56..−0.60 ms** at 10,300 draws (three
independent arms of the fixed code agree to 0.04, se ~0.1), wall −0.35..−0.5. Gotcha 641.

## §2. The part-109 bundle, re-measured with `cz-draw` as the bound

`CZ_VK_TEXMEMO=1 CZ_VK_SCOPED_SHARED_ZERO=1` vs without, same binary (`cz_pf`), three runs
a side: **`cz-draw` 11.06 -> 9.97 (−1.09 ms, se 0.1)**, wall −0.5, Main/Draw unchanged;
`[texmemo]` 78.7-79.0% served, **0 disagreements**. Part 117 measured −0.56 on the thread
and −0.13 on the frame because the Main Thread was the bound then; the thread's saving has
doubled with the draw count. **Still OFF — the ship decision is the operator's** (part
109's rule); the recommendation is ON.

## §3. Two nulls, both reverted

* **Prefetch in `FlatCache::Find`** (`perf annotate`: ~73% of UploadStream's self time on
  four instructions after the probe's loads — the gens/keys/vals arrays are three lines in
  three places). Prefetching the key and value lines at the hash: wall +0.03, cz-draw
  −0.02..−0.13, not monotone. The core's out-of-order window already overlapped them.
  A struct-of-slots layout is the untried form; not built.
* **Timer slack on the pump thread** (§4). Measured: D's idle unmoved (0.8-1.2 vs
  0.9-1.6 ms/frame), wall within noise. Reverted.

## §4. `cz-draw` idles ~1 ms a frame waiting for the GUEST, not for our nap

~47-60 WAIT_REG_MEMs a frame on `BC739E80` are unmet with one of our stores to that word
pending, and only ~10.6 are satisfied by the pending value (`PendingStoreValue` checked:
its endian path equals `LoadGpu` after landing). The rest wait on a value nothing in the
stream writes — the guest's. The walk holds, `cz-draw` runs dry behind it.

**A true fact found on the way, recorded because it bites every timed nap**: Linux
stretches a timed wait by the thread's timer slack, 50 us by default. A standalone
`condition_variable::wait_for` on the dev box: asked 5 us -> 58.9, 10 -> 63.3, 100 ->
154.1; with `PR_SET_TIMERSLACK` 1 ns: 9.7 / 14.7 / 105.9. So the pump's fast-retry
ladder's "5 us" rung has always been ~60 us on Linux, and the 100 us tick ~154. Fixing it
measured a null here because this wait is not the nap's latency (gotcha 642).

## §5. The reader: fit, don't band, on this route

The band reader (`part116_guestcpu.py`) said the shadow-bias fix was "wall −1.07 ms,
monotone over 6 bands". The fit over every window said −0.35..−0.5. The per-window wall
MEDIAN moves in 1 ms vblank steps (gotcha 572), and the route lands in different bands
every run, so a band can be n=1 a side. `tools/part122_fit.py` fits cost against draws per
arm and reads every column at one load with a standard error. Quote `cz-draw` first.

## §6. What the detail defaults cost (two runs a side, informational)

Bundle on in both arms: WORLD/ZOMBIE DETAIL 2.0 soaks at 10,600-11,000 draws, 10.9-12.9
ms (78-92 fps), `cz-draw` 9.5-10.3; both at 1.0 (`CZ_WORLD_LOD=0 CZ_CROWD_LOD=0`) at
7,600-8,200 draws, 8.5-9.3 ms (107-118 fps), `cz-draw` 7.8-8.6. **The 2.0 defaults cost
~2-3 ms a frame (~25% fps) at the crowd.** One 1.0 run failed the route's 8,000-draw gate
for that reason (kept as `.rejected`). The settings are the operator's choice.

## §7. What is owed / next

1. The operator's call on the bundle (§2): −1.09 ms on the bound thread, verified, 0
   disagreements. Flipping it means renaming the arms to `CZ_VK_NO_TEXMEMO` /
   `CZ_VK_FULL_SHARED_ZERO` (the latter already exists as the scoped arm's control).
2. `cz-draw` is still the bound (~10 ms with the bundle) over Main (8.3-8.5). The
   remaining lever that moves work OFF it: let `cz-pump` (W, ~32% of a core) compute each
   draw's lookup KEYS (texture memo keys, stream keys, pipeline key hash) from its own
   register file at the packet and carry them in the `DrawRec`, so `cz-draw` only looks
   up — and can prefetch draw N+1's slots from keys already in the log. Not built.
3. The guest-coupled idle (§4): name the guest code that writes `BC739E80`
   (`CZ_WAIT_CALLERS=1`-style census on the store) before designing anything.
4. Gates for any of the above before release: `tools/part117_gates.sh`.
