# Case Zero Recomp: heap-growth build

A fork of [Dead Rising 2: Case Zero Xenon Recomp](https://github.com/wivi514/Dead_Rising_2_Case_Zero_Xenon_Recomp) v1.1.3 that gives the game more memory for its streaming and audio heaps, so that **WORLD DETAIL 4.0 / ZOMBIE DETAIL 5.0** play without the pop-in and missing sounds the stock build has at those settings. It also adds a setting that loads items from farther away.

Everything is on by default in the AppImage and can be overridden from the environment. Unset everything (or use the stock v1.1.3 release) to get the original behaviour.

## What it changes

The title takes one 447 MB block of the 360's 512 MB at boot ("heap 0") and carves about 78 fixed-size heaps from it. At high detail, two of them run out:

- **The SW Texture heap** (the world streamer's memory). The streamer drops to "low memory" mode and stops loading detail, which causes pop-in and items that unload and reload.
- **The audio heaps.** You get `Out of Audio Sample Memory` and missing item sounds.

About 56 MB of the emulated 512 MB was never used by anything, so this build:

| Setting (env var) | Default | What it does |
|---|---|---|
| `CZ_MAIN_BLOCK_GROW_MB` | 50 | Asks for 50 MB more in the title's boot block (heap 0) |
| `CZ_SW_TEXTURE_GROW_MB` | 38 | Streaming heap (pop-in) +38 MB |
| `CZ_AUDIO_MAIN_GROW_MB` | 5 | Audio Main heap +5 MB |
| `CZ_AUDIO_BANK_GROW_MB` | 1 | Each audio bank sub-heap +1 MB (taken from the Audio Main growth) |
| `CZ_STREAMING_WORLD_GROW_MB` | 6 | Streaming world heap +6 MB (parent of the prop-audio heap) |
| `CZ_PROP_AUDIO_GROW_MB` | 4 | Prop/item sound heap +4 MB |
| `CZ_ITEM_DISTANCE` | 2 | Every item's load distance (`items.txt` `LoadDistance`) ×2; 1 = the game's own |
| `CZ_ITEM_DISTANCE_OVERRIDE` | `mailbox=250` | Per-item load distance in metres, replacing the ×2 for matching items. Comma-separated `name=metres`, where the name is a case-insensitive part of the item's model path or short name (e.g. `mailbox`, `garbagecan`, `newspaperbox`). Mailboxes (35 m stock) faded in close to the player even at ×2 |
| `CZ_NO_ZOMBIE_HEAP_GROWTH` | 1 | Keeps the stock build's 16 MB modding loan to the Zombie heap off, so the streaming heap keeps it |

Measured at WORLD DETAIL 4.0 / ZOMBIE DETAIL 5.0 (the `CZ_HEAP_CENSUS=1` log, below):

| | Streamer checks in low-memory mode | Audio sample failures |
|---|---|---|
| stock v1.1.3 | 72.5% | yes |
| this build | 0–0.5% | 0 |

Outdoors at those settings there is no noticeable pop-in, and objects no longer render see-through before you get close.

## Supersampling (recommended if your GPU has headroom)

The port can already render above your screen's resolution and average it down; this is cleaner than MSAA. Set it as a launch option (Steam: `CZ_VK_RES=5120x2880 %command%`), in borderless or fullscreen mode:

| `CZ_VK_RES` | Pixels vs 1440p | Result |
|---|---|---|
| `5120x2880` | 4× | aliasing essentially gone (exact 2×2 average) |
| `3840x2160` | 2.25× | minor aliasing left, still better than MSAA 4× |

On a Ryzen 7 9800X3D + RX 9070 XT at 2560×1440, frame rate was the same with and without it. Crowds are limited by the CPU, not the GPU (around 60 fps in big crowds at ZOMBIE DETAIL 5.0, with or without supersampling or MSAA). Lowering ZOMBIE DETAIL is what raises crowd frame rate. The maximum is 2880 lines tall, and in windowed mode the window grows to the internal resolution instead of averaging down.

## Known limit

The **general store** still empties some shelves as you walk around inside, for example the orange juice at the far wall and two shelves in a corner. Xenia does exactly the same, so it is the original game's behaviour. Memory, item distance, an item cap, occlusion culling and the scenery detail switch were each tested and ruled out.

## Diagnostic switches (all off by default)

| Env var | What it does |
|---|---|
| `CZ_HEAP_CENSUS=1` | Logs every heap's size, free space and largest free block, the streamer's level, and audio failures per heap, every 15 s (`CZ_HEAP_CENSUS_S=N` to change) |
| `CZ_DETAIL_MIN_SWITCH=M` | Raises scenery pieces authored at ≤ 2 m switch distance (shop products, small props) to M metres before WORLD DETAIL multiplies. `CZ_DETAIL_MIN_FROM=5` widens it to the 5 m pieces, which costs a lot of memory |
| `CZ_VOLUME_TRACE=1` | Logs every scenery piece switching between its detailed and far version, by name |
| `CZ_ITEM_DIST_TRACE=1` | Logs item load distances as the game reads them, with each item's name (the names `CZ_ITEM_DISTANCE_OVERRIDE` matches) |
| `CZ_NO_OCCLUDERS=1` | Loads zones with no occluders (occlusion culling off) |
| `CZ_PROP_TRACE=1` | Logs dynamic prop IDs being allocated and freed |

## How it works (for porters)

- `runtime/cpu/heap_growth.cpp` grows the boot block by hooking `sub_825D2688`, the title's physical alloc wrapper, which has one caller. It also hooks `sub_8277BF48`, which receives the size separately. The sub-heaps are grown in block/heap pairs: the block comes from `sub_8277F0B8` and the heap from `sub_8277DD40`, matched at fixed call sites.
- `runtime/cpu/heap_census.cpp` reads the title's heap tables (`0x82A8A070` live, `0x82A73ED0` size, `0x829ECD28` name). It calls the title's own free-space queries, `sub_8277C288` and `sub_8277C1A8`, and hooks the streamer's memory check, `sub_8222BD70`.
- `runtime/cpu/item_distance.cpp` hooks `sub_823E24D0`, the only reader of an item def's `LoadDistance`. It was found with a hardware read-watchpoint.
- `runtime/cpu/volume_trace.cpp` hooks the per-volume scenery vote, `sub_82175040`.

## Install

Put `CaseZeroRecomp-heap-linux-x86_64.appimage` in the same folder as your existing Case Zero data (the folder with `assets/`) and run it. It uses your existing saves and settings. The system's libstdc++ is used, so no `LD_PRELOAD` is needed.
