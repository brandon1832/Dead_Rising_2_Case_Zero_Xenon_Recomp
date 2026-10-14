# Final state — the project ended at v1.1.3 (2026-10-14)

The maintainer stopped working on this port after v1.1.3: *"I won't work on this project
anymore so 1.1.3 is the official last update from me."* This file is the hand-off for
anyone who picks it up. It says what was shipped, what is open (with where each open
item's record lives), what you need in order to build, and what will not exist for you
any more. Everything else in `docs/` is the working record that this file points into.

## 1. What shipped

**v1.1.3 is the last release.** Tag `v1.1.3` = `51df8c7`. Five downloads plus
`SHA256SUMS`; `docs/release-notes-v1.1.3.md` has the text, the hashes and the gate
record.

| download | built from | how |
|---|---|---|
| `CaseZeroRecomp-linux-x86_64.tar.zst` / `.AppImage` | `a0e4f35` | `tools/release_build_oldbase.sh` (Ubuntu 22.04 container, glibc 2.35 floor), `tools/release_package_appimage.sh` |
| `CaseZeroRecomp-steamdeck-x86_64.tar.gz` | `a0e4f35` | `tools/release_build_steamdeck.sh` (`CZ_DECK`: no launcher, 1280x800, detail settings at 1.0) |
| `CaseZeroRecomp-windows-x86_64.zip` | `9191cb0` | `tools/release_package_windows.ps1` on Windows, `docs/windows-build-setup.md` |
| `CaseZeroRecomp-macos-arm64.zip` | `bcaed0d` | `tools/release_package_macos.sh` on an Apple Silicon Mac, `docs/macos-build-setup.md` |

The commits after `a0e4f35` change nothing on Linux (the DPI fix and the macOS changes
compile to nothing there), which is why the Linux legs were not rebuilt.

**The state of the game:** completable start to finish, solo and in two-player online
co-op (XenonLive), on Windows and Linux; the Steam Deck build and the macOS build are
the least tested. 60 fps, level cap 50 with all fifteen skills, the PC options screen,
ultrawide, keyboard and mouse, MSAA, the three v1.1.3 detail settings.

## 2. What you need to build it

1. **Your own copy of the game** (the 825 MB XBLA package). Nothing in this repository
   is game data; see `README.md`.
2. **The patched recompilers.** `patches/README.md`: the 14 XenonRecomp and 23
   XenosRecomp patches, exported from the maintainer's checkouts, with the upstream base
   commit each applies to. Without them the generated code and the shaders are wrong.
3. **XenonLive — the blocker.** `runtime/CMakeLists.txt` requires a XenonLive checkout
   at configure time, and that repository (`wivi514/XenonLive`) is **private**. Without
   access, the first thing to do is make it optional (`patches/README.md` says where the
   seams are). Nobody has built the runtime without it.
4. Then the build order in `CLAUDE.md` (Commands): unpack, `ppc/` from
   `config/CaseZero.toml`, the shader cache (the runtime builds it itself on first run),
   cmake.

## 3. What is open

Ordered roughly by how often a player would notice. Each line points at the record; read
that before re-deriving anything, and read `docs/gotchas.md` before believing any
number.

| item | state | record |
|---|---|---|
| macOS freezes after ~15 min left idle (sound continues, 0 fps); ~40 fps on an M1 | open, never profiled on the release build | `open-items.md` 0zk, `macos-build-setup.md` |
| macOS is not notarised | players must run `xattr -dr com.apple.quarantine` once, or the app is translocated and cannot find `assets/` | `release-notes-v1.1.3.md`, `release-plan.md` C.4 |
| the "LAS VEGAS 46" sign shows holes while the camera moves | parked; 11 arms refuted; next step a draw-ID F9 of a bad frame | `las-vegas-sign-holes.md`, `open-items.md` 0zj |
| rooftop props / sign lettering pop at streaming boundaries | open, never compared at WORLD DETAIL 1.0 | `lod-and-quality-options.md` §5, `open-items.md` 0zi |
| highlight brightness measures ~half what the engine expects | open | `lighting-plan-part120.md`, `phase5-notes.md` §6fc |
| performance: the renderer thread `cz-draw` is the frame's bound (~10 ms at a crowd) | a to-do list, ordered | `perf-plan-part122.md` §7 = GitHub issue #2 |
| the texture memo + scoped zero bundle (ON since `1b14044`) never had its 10-minute soak | owed; `CZ_VK_NO_TEXMEMO=1` / `CZ_VK_FULL_SHARED_ZERO=1` are the first bisection for any wrong texture or wrong vertices | `perf-plan-part122.md` |
| co-op: a guest who arrives dressed but unloaded is repaired after the fact; nothing upstream asks for his files | backstop shipped, cure open | `coop-plan.md` Part 13, `open-items.md` 0ze |
| co-op: joining a friend through the title's own path | parked, two title bugs deep | `coop-plan.md` Part 7, `open-items.md` 0zb |
| the pawnshop wheel respawns after being taken | happens in single player too; believed the title's own, unchecked on a real 360 | `coop-part14-kickoff.md` §1 |
| loud audio after ~40 minutes (one player report) | parked | `open-items.md` 0za |
| 21:9 HUD anchoring | parked on branch `hud-anchor-wip` | that branch |
| a shadow-distance option; RT shadows | parked | `shadow-distance-investigation.md`; `open-items.md` 0v |

## 4. What will not exist for you

- **The maintainer's machines.** `ssh czwin` (Windows build laptop), `ssh czmac` (the
  M1), `ssh czamd` (an AMD test PC) appear throughout the docs; they were personal
  hardware. The runbooks still say exactly what each needed.
- **The operator.** Most picture and co-op verdicts in this record were the
  maintainer playing the game ("operator-verified"). Headless instruments exist for a
  lot, but the docs are explicit wherever only a human eye settled something.
- **Case West.** `CLAUDE.md` says Dead Rising 2: Case West was planned next on the same
  engine. It will not be done by this maintainer. `docs/reusability.md` is what would
  transfer.

## 5. Housekeeping left as it was

- Commit `5117597` pushed ~233 MB of `dist-steamdeck` build output into this public
  repository's history. It was untracked in `8917e8e`; removing the blobs needs a
  history rewrite, which was the maintainer's call and was not made.
- `CLAUDE.md` is the development journal and is long; its first section now says the
  project ended. The numbered ledgers (`xenia-capture-analysis.md`, `gotchas.md`) and
  `open-items.md` are the parts worth reading first.
