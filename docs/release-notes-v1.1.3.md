# Release notes — v1.1.3

**PUBLISHED 2026-10-14; non-Deck artifacts rebuilt 2026-10-14 from `b3895ad`.** Tag
`v1.1.3` is on the rebuild. Linux tarball + AppImage (old base, glibc 2.35 floor),
Windows (czwin) and macOS (czmac) now default WORLD DETAIL to 1.0. The Steam Deck build
already used 1.0 and its archive is unchanged. XenonLive `134d49b` / launcher overlay
`e59d3ed` remain on the Windows build; macOS is ad-hoc signed and passed the staged
`--smoke` gate. All replacement artifact hashes are recorded below.

**Gates, 2026-10-13:** Linux release `.text` byte-identical to its matched RelWithDebInfo
build (old-base toolchain); tarball and AppImage GATE PASSED in `ubuntu:22.04` (glibc 2.35
floor); Steam Deck GATE PASSED with `CZ_GATE_SYSTEM_CXX=1`, and its build compiled with
`-DCZ_DECK_DEFAULTS` (verified in its `build.ninja`); Windows staged-exe `--smoke` OK and
the zip's hash identical after the copy from czwin. **Old-save gate (overlay v7 + `a3ade42`):**
a save written 2026-09-23 (before v7) loaded into gameplay on the STAGED Linux binary over
its v7 overlay — 42 `CreateZombiesCallback` lines, headless (`CZ_SAVE_DIR` = a copy).
**NOT run:** the 10-minute soak owed by `1b14044`'s texture memo + scoped zero bundle, and
no human has played the staged builds yet.

Folded from `docs/release-notes-next.md`, plus the items that never
reached that draft (co-op issue #11, the modding toolkit hooks, the iGPU white-screen
import, `--debug-menu`).

**The body is the text BELOW the first `---`; the header above it is internal.** If any
artifact is ever rebuilt, refresh its hash below and in `SHA256SUMS` before attaching.

---

# ⚠️ ONLINE PLAY — READ THIS FIRST

> [!WARNING]
> **DO NOT launch the game executable directly if you want online features.**
>
> Launch the game through the **[XenonLive Launcher](https://github.com/wivi514/XenonLive_Launcher)** for:
>
> **🎮 Online / Co-op · 🏆 Achievements · 👤 Gamertag · 👥 Friends**
>
> Starting the game any other way = **OFFLINE MODE ONLY**.

> **This is the final release.** The maintainer has stopped working on the port, so no
> further updates are planned and issues may go unanswered. The code, the recompiler
> patches and the whole development record stay in the repository for anyone who wants
> to continue it (`docs/final-state.md`).

Co-op can now be played to the end, three new detail settings bring distant zombies,
buildings and shadows closer to what you see up close, Chuck's hair stops flickering, and
the renderer is about 1 ms a frame faster in big crowds.

**Upgrading from v1.1.2:** unpack over your existing folder, or let the launcher install
it — settings and saves live outside the game folder and are untouched. Your unpacked
game data is reused, and **your existing saves load normally**.

> **First launch after this update does two one-off preparation passes** and then never
> again: **PREPARING SHADERS** (part of the shader cache is rebuilt — this is the hair
> fix reaching your install) and a few seconds of regenerating the patched game data
> (for the co-op ending). Both are the update working, not a hang.

### 🍎 New: macOS (Apple Silicon)

The first macOS build: `CaseZeroRecomp-macos-arm64.zip`, for Macs with an M1 or newer
chip. It is an **early build**: tested on one M1 MacBook Air, where it plays, but expect
rough edges (see Known limitations).

1. Unzip it, then move the `CaseZeroRecomp` folder wherever you want to keep it.
2. **Before the first launch**, open Terminal and run this once (drag the folder onto the
   Terminal window to fill in its path):
   ```
   xattr -dr com.apple.quarantine ~/Downloads/CaseZeroRecomp
   ```
   The build is not notarised by Apple, so macOS marks the download as untrusted. Without
   this step macOS either refuses to open it or runs it from a hidden temporary location
   where it cannot find its `assets` folder.
3. Open `CaseZeroRecomp.app`, then drop your Case Zero package onto the launcher window
   (or put it in `assets/package/` first). The first launch unpacks the game and prepares
   the shaders, which takes a few minutes.

Keep `CaseZeroRecomp.app` and the `assets` folder **together in the same folder**. Saves
go to `~/Library/Application Support/Dead Rising 2 Case Zero/`.

### Co-op

- **The ending is now playable in co-op.** The host no longer crashes when the military
  arrives (the joiner's game was removing the host's helicopters out from under their
  pilots), and **each player now rides his own motorcycle** to the end and sees the
  ending cutscene — the guest used to be left on foot. Single player is unchanged.
- **Items stay the same item on both machines.** Picking up something in co-op could hand
  the other player a different item (the host's engine became the guest's forks), because
  the two games spawned items in a slightly different order. Both machines now agree.
- **The "Find Bike Parts" tracker shows both players' parts,** not only the ones the
  local player carries.
- **A guest with no save is no longer invisible on the host.**

### Visuals

- **New setting: ZOMBIE DETAIL** (Settings panel). Zombies switch to their detailed model
  from twice as far away, and twice as many get it at once — before, you had to stand
  right next to one. Original game: 1.0; default now 2.0, up to 5.0. Higher costs more
  GPU time in big crowds. The Steam Deck build keeps 1.0.
- **New setting: WORLD DETAIL** (Settings panel). Buildings, rooftop props and sign
  lettering can switch to their detailed version sooner. Original game and default:
  1.0, up to 4.0. This v1.1.3 rebuild restores 1.0 as the default on Linux, Windows
  and macOS. The Steam Deck build already defaults to 1.0.
  Some rooftop props and sign lettering can still pop as you cross a streaming boundary;
  that also happens at 1.0, just closer to you.
- **New setting: SHADOW BIAS** (Settings panel). Shadows of awnings and other thin
  overhangs are now visible from a distance instead of growing in as you walk under them.
  Default 0.25; 1.00 is the original game's value.
- **All three apply immediately**, no restart.
- **Chuck's hair no longer flickers at the back of the neck.** Two of the hair's drawing
  passes computed the head's position slightly differently, so the layers traded places
  from frame to frame. Other characters drawn the same way are fixed too.
- **Soldier zombies keep the same head and helmet at every distance.** The game's far
  soldier model had two parts misnamed (a real 360 does the same), so every soldier wore
  a helmet from afar and many lost it up close.
- **Reflections are complete at resolutions above 720p.** Part of every reflection showed
  leftover memory instead of the scene (dull on most PCs).

### Performance

- **About 1 ms less work per frame on the renderer thread in big crowds**: repeated
  texture lookups are remembered within a frame and less memory is cleared per draw.
  Checked against the old path on 267 million texture lookups with zero differences. If
  a texture ever looks frozen or wrong, launch with `CZ_VK_NO_TEXMEMO=1` and tell us
  whether it goes away.

### Other

- **Windows: the Resolution list now reaches your screen's real resolution** when
  Windows display scaling is above 100% (most laptops). Started through the launcher,
  the game was being told the scaled-down desktop size — a 2560x1440 screen at 150%
  read as 1707x960 — and offered nothing above 1600x900.
- **Integrated-graphics laptops that showed a white window with sound** (reported on Dead
  Rising 2: Case West, which shares this code): the game now retries without the
  driver's switchable-graphics layer, and if the renderer still cannot start it says
  which step failed, on screen and in the bug report.
- **`--debug-menu`** on the command line enables the debug menu (F2 / F4), so a Windows
  shortcut can carry it.
- **Support for the DR2 modding toolkit**: replacement files in `assets/game_mods/`
  (textures, models, sounds) are loaded over the game's own, and the game has room for
  the extra shaders that imported items bring. Nothing changes if the folder is empty;
  `CZ_NO_MODS=1` turns it off.

### Known limitations

macOS (new, early): it can freeze if left idle for about 15 minutes (sound continues),
and runs at about 40 fps on an M1 MacBook Air; Intel Macs are not supported. The Linux artifacts need glibc
2.35 or newer (Ubuntu 22.04 and later, or the AppImage). Highlight brightness is still
not exactly right. The pawnshop's wheel can be taken twice; that happens in single player
too and appears to be the original game's behaviour.

### Artifacts

| file | sha256 |
|---|---|
| `CaseZeroRecomp-linux-x86_64.tar.zst` | `36bbed757b83f266434299d8cbaf32b4bfab5331b4a7e1ced3d34ab633cd8bf8` |
| `CaseZeroRecomp-linux-x86_64.AppImage` | `6ec1d521258201b22ff373b384caec35bb85d100540e7150cb3f11c73230aacf` |
| `CaseZeroRecomp-steamdeck-x86_64.tar.gz` | `72bf4773a267a963d92f424529b3a2d237f4deeb89244a10b88e2523e4d1613b` |
| `CaseZeroRecomp-macos-arm64.zip` | `79cf10630afca24d5443cb91adcc9aad793fde8986a7e3baa4c630b9e86f62db` |
| `CaseZeroRecomp-windows-x86_64.zip` | `f821dd717c08baa83b06ae11ece1963835bbe07816b43a52792f3040cfd99a28` |
