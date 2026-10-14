# Patches to the recompilers

This port is built with **patched** copies of hedge-dev's
[XenonRecomp](https://github.com/hedge-dev/XenonRecomp) and
[XenosRecomp](https://github.com/hedge-dev/XenosRecomp) (both MIT). The patches lived
only in the maintainer's local checkouts, whose one remote is upstream. When the project
ended at v1.1.3 they were exported here, because nobody can rebuild the port without them.

| directory | patches | upstream base commit |
|---|---|---|
| `XenonRecomp/` | 14 | `ddd128bcca99fe8bfbb99bea583c972351fa6ace` |
| `XenosRecomp/` | 23 | `990d03b28a27b50277ee5d8d942e1c5f873869d1` |

```
git clone https://github.com/hedge-dev/XenonRecomp && cd XenonRecomp
git checkout ddd128bcca99fe8bfbb99bea583c972351fa6ace
git am /path/to/this/repo/patches/XenonRecomp/*.patch

git clone https://github.com/hedge-dev/XenosRecomp && cd XenosRecomp
git checkout 990d03b28a27b50277ee5d8d942e1c5f873869d1
git am /path/to/this/repo/patches/XenosRecomp/*.patch
```

Clone them as siblings of this repository (`~/GithubRepo/XenonRecomp`,
`~/GithubRepo/XenosRecomp`), which is where `runtime/CMakeLists.txt` looks by default,
or pass `-DXENON_ROOT=` / `-DXENOS_ROOT=`.

**Checked when exported (2026-10-14):** both sets apply with `git am` to a fresh clone at
the base, and every file they change comes out byte-identical to the checkouts the
v1.1.3 artifacts were built from. A few of the original commits had also committed
`build*/` directories by accident (object files, a 95 MB precompiled header); those
paths were excluded from the export, so the patches carry source only.

What each patch is for: `docs/xenonrecomp-upstream-bugs.md`, and the commit message of
each patch.

**XenonLive is not in this directory, and the build needs it.** The online layer
(`libxlive`, and the launcher's in-game overlay) comes from two separate public
repositories, cloned as siblings like the recompilers:

```
git clone https://github.com/wivi514/XenonLive           # XLIVE_ROOT, required at configure time
git clone https://github.com/wivi514/XenonLive_Launcher  # XLIVE_LAUNCHER_ROOT, the overlay (optional)
```

The released v1.1.3 was built with XenonLive `134d49b` and XenonLive_Launcher `e59d3ed`.
