# Release notes — v1.1.3

**DRAFT — built and gated, NOT PUBLISHED.** Binaries are master at `a0e4f35`, built
2026-10-13, all four legs: Linux tarball + AppImage (old base, glibc 2.35 floor), Steam
Deck (`CZ_DECK` defaults: ZOMBIE/WORLD DETAIL 1.0), Windows (czwin). XenonLive `134d49b` /
launcher overlay `e59d3ed` on BOTH platforms. Gate results and hashes are filled in below
as each leg passes.

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
  lettering switch to their detailed version from twice as far away — the pawnshop's
  sign, the cactus and the fire hydrant used to appear only once you were across the
  street. Original game: 1.0; default now 2.0, up to 4.0. The Steam Deck build keeps 1.0.
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

macOS is not released yet (it runs, but is not ready). The Linux artifacts need glibc
2.35 or newer (Ubuntu 22.04 and later, or the AppImage). Highlight brightness is still
not exactly right. The pawnshop's wheel can be taken twice; that happens in single player
too and appears to be the original game's behaviour.

### Artifacts

| file | sha256 |
|---|---|
| `CaseZeroRecomp-linux-x86_64.tar.zst` | `163683934e124eb276257ee2edfe40f189d5af0460e734cf4b9834f09c207eb8` |
| `CaseZeroRecomp-linux-x86_64.AppImage` | `ab1a655aaa151128bad4ffe23b25b6a8e68423e2dfc2712cd07ee5ffa77e394c` |
| `CaseZeroRecomp-steamdeck-x86_64.tar.gz` | `72bf4773a267a963d92f424529b3a2d237f4deeb89244a10b88e2523e4d1613b` |
| `CaseZeroRecomp-windows-x86_64.zip` | `04b71a4fba0b3a35189ed35d81123a425f9a3a5694589299791038b176469dc8` |
