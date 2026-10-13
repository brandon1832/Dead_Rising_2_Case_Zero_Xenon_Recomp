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
`[texmemo]` 78.7-79.0% served. **RETRACTED IN PLACE: the "0 disagreements" on that line
is NOT a verification** — `g_texMemoDisagree` only counts under `CZ_VK_TEXMEMO_VERIFY=1`,
which no part-122 run set, so it reads 0 by construction (gotcha 151's shape). The
verification on record was part 109's: 0 over 718 million served lookups, on an older
build. Part 117 measured −0.56 on the thread and −0.13 on the frame because the Main
Thread was the bound then; the thread's saving has doubled with the draw count.

**§2.1 — VERIFIED AND FLIPPED ON (`1b14044`, the operator's call, 2026-10-11).** On the
flipped build, the crowd route: `CZ_VK_TEXMEMO_VERIFY=1` read **0 disagreements of
267,406,653** verified hits, and `CZ_VK_TEXMEMO_VERIFY_POISON=1` (new, `d80fe63`: compare
against a deliberately wrong slot) read **141,184,658 of 141,184,659** — the counter is
shown able to fire. The controls are now `CZ_VK_NO_TEXMEMO=1` and
`CZ_VK_FULL_SHARED_ZERO=1`; both print their state once at boot. Gates on that binary
(`tools/part117_gates.sh`): part47 ALL CLEAN, E3 +0.8319 (4 of 5 agreed; part 118 read
+0.8415); A5 5 permutation / 0 real; sync validation 0 hazards at 6,249 draws, poison 30;
truncated 0. **Not run (the machine was needed): gate 5, the 10-minute explorer soak, and
part47 with both controls set** — §7 item 0.

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

## §7. TO-DO FOR THE NEXT PERFORMANCE PERSON (mirrored as GitHub issue #2)

Written 2026-10-13 as a hand-off: someone else may pick up performance from here. Each
item says what is known, what to measure first, and what would kill it. Read §0 and §5
first, and `docs/gotchas.md` 446-447, 572, 641-642.

**Where the frame is (bundle on, 2.0 detail defaults, ~10,300-11,000 draws at the
crowd, 1920x1080, Ryzen 7 5700):** `cz-draw` ~10.0 ms (the bound), guest Main 8.3-8.5,
guest Draw Thread ~6.4, `cz-pump` ~3.5, GPU 3.8 with fence wait 0.02. So the next
~1.5 ms comes off `cz-draw`; past that the guest's Main Thread is the bound again
(part 116 §4.1, part 118 §1) and renderer work stops paying.

**How to measure (the method is half the job here):**
- Route: `tools/part80_crowdroute.sh` (operator-route replay, 9-11k draws, unattended).
- Same-binary env arms: `tools/part122_campaign_env.sh <bin> 3 base "new:CZ_FOO=1"`;
  two binaries: `tools/part122_campaign.sh`. **Three runs an arm, alternated**
  (noise floor 10-13% at one run, gotcha 229). Print the CPU clock (the scripts do).
- Read: `tools/part122_fit.py 10300 base="$OUT/crowd_*_base?.log" new="..."` — a linear
  fit per arm read at one load. **Quote `cz-draw` (the `pump cpu` column) first; the wall
  median is quantised to 1 ms by the vblank and cannot read sub-ms changes (gotcha 572).**
- Profile: `perf record -g -p <pid>` for 20 s during the soak, then filter to the
  `cz-draw` TID. The phase profiler (`CZ_VK_PROFILE`) has misattributed three times;
  `perf` wins (`tools/phase_vs_perf.py`).
- Gates before any default changes: `tools/part117_gates.sh` (all five steps).
- Any new instrument needs its own positive control that is shown to fire (gotcha 30,
  and §2's retraction for what happens otherwise).

**0. Finish the gates on `1b14044` (cheap, do first).** Gate 5 of
`tools/part117_gates.sh` (10-minute `CZ_AUTOCHUCK=EXPLORER` soak with
`CZ_WAIT_TRACE=1`: no crash, no named infinite wait). Then
`CZ_VK_NO_TEXMEMO=1 CZ_VK_FULL_SHARED_ZERO=1 tools/part47_gates.sh` to say whether E3
+0.8319 vs part 118's +0.8415 is the flip or run-to-run spread (expected: spread; the
memo verified exact and the scoped zero's poison sat inside the picture null).

**1. Move lookup-key computation off `cz-draw` onto `cz-pump` (the structural lever,
est. 1-2 ms, not built).** `cz-pump` (the PM4 walk) is ~32% of a core; `cz-draw` is
96%. Today `cz-draw` reads the replica register file and computes every per-draw key
itself: texture memo keys (24-byte fetch-constant compares x slots), stream-cache keys
(FlatCache hash in `UploadStream`, whose probe stalls are ~73% of its self time), the
pipeline key hash. W already holds the same registers at the packet. Have W compute the
keys and carry them in the `DrawRec` (`gpu/pump_split.{h,cpp}`), so D only looks up —
and, with draw N+1's keys already in the log, can prefetch N+1's FlatCache slots while
drawing N (the prefetch that was a null in §3 was issued at the hash, too late to
overlap). Risks: the DrawRec grows (it is copied through the ordered queue); keys must
be computed from exactly the register state D would have seen (the replica is updated
in stream order — keep it that way). Kill: `cz-draw` −0.4 ms at 10,300 draws or less
on the fit after three runs an arm.

**2. `cz-draw`'s ~1.0-1.3 ms/frame idle is a wait on the GUEST (§4) — name the writer
first.** W holds on WAIT_REG_MEM against word `BC739E80`; ~47-60 waits a frame, only
~10.6 satisfied by our pending store. Before designing anything: find which guest code
writes that word (a store-site census like `tools/find_field_access.py`, or a
`CZ_WAIT_CALLERS=1`-style trace on the store) and whether the Draw Thread could be fed
sooner. If it is the guest's own pacing, this item is closed.

**3. FlatCache as struct-of-slots (untried form, est. ≤0.5 ms).** The stream cache's
`FlatCache` keeps gens/keys/vals in three separate arrays, so one probe touches three
cache lines in three places; prefetching them was a null (§3) because the OoO window
already overlapped the loads. Packing (gen, key, val) into one 64-byte-aligned slot
makes a probe one line. Cheap to try; measure on the fit, `cz-draw` column.

**4. The remaining `cz-draw` profile, for whoever wants a smaller item** (`perf`, user
time, part 122, before the bundle): `DoDraw` self 19%, `UploadStream` 13%, the split's
replay loop + spin 9%, texture lookup ~11% (now mostly memo hits), the const-window copy
7%, the driver 8%. Re-profile on `1b14044` before picking — the memo moved the texture
share. **Never read `R->arena.mapped` (write-combined, uncached; gotchas 446-447, 641):
every read is a DRAM round trip.** Grep any new hot code for loads from the arena.

**5. Known non-items — do not re-buy without new evidence:** FlatCache prefetch at the
hash (§3); timer slack on the pump (§3/§4, gotcha 642 — true fact, wrong wait); moving
the shared-block memset to idle cores (part 111, gotcha 551); PGO/ThinLTO/-O3 on `ppc/`
(part 116); a fourth Havok worker (part 118, gotcha 573); parallel command recording
(part 80, gotcha 473); vectorising the byte swap (part 109, gotcha 545).

**6. Open elsewhere, owed but not on the Linux crowd frame:** macOS ~40 fps + a freeze
when idle (`open-items.md` 0zk); the Windows guest-thread clocks read ~0 and the
six-core/no-SMT pin shapes (part 118 owed list); the detail settings' own price — 2.0
vs 1.0 is ~2-3 ms a frame (§6), a settings decision rather than a code item.
