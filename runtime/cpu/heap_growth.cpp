// Opt-in heap growth for high detail settings: a bigger main block, and from it a bigger
// SW Texture heap (the streaming world), Audio Main heap and audio bank sub-heaps.
//
// WHY THIS EXISTS (2026-10-25, the "more streaming and audio memory" fork). WORLD DETAIL
// and ZOMBIE DETAIL above the 360's budget run the streamer out of memory (pop-in) and the
// audio library out of sample memory (missing sounds). Every heap the title uses is carved
// from ONE block it takes at boot, so a heap can only grow if its parent has room.
// cpu/heap_census.cpp (CZ_HEAP_CENSUS=1) is the measurement that says where the room is.
//
// THE CHAIN, each link read from the title's own code (tools/gdis.py):
//   * sub_82496D98 asks sub_825D2688(size) for 0x1BF16000 bytes of physical memory (447
//     MB), stepping down 1 KB per failure, and makes heap 0 of whatever it got
//     (sub_8277BF48). sub_825D2688 is a 4-instruction XPhysicalAlloc wrapper with that one
//     caller. Our physical arena is 511.94 MB, so whatever the title and the runtime do
//     not take after boot is the only memory not already spoken for.
//     CZ_MAIN_BLOCK_GROW_MB=N asks for N MiB more, and falls back to the title's size if
//     the arena cannot give it. This alone changes nothing a player sees: it is heap 0's
//     free space, which the next three draw on.
//   * The SW Texture heap's size is sub_82483860 (one caller, the boot-time heap setup),
//     its block taken from heap 0. CZ_SW_TEXTURE_GROW_MB=N adds N MiB (after the Zombie
//     heap's share is taken, cpu/zombie_heap.cpp).
//   * The Audio Main heap is sub_82483888() + 0x1800000, one caller (sub_825D7448 ->
//     sub_8245E2B0, "Audio Main heap", block from heap 0). CZ_AUDIO_MAIN_GROW_MB=N adds N.
//   * The audio store (sub_8275E178) carves up to ten "AudioBankHeap%d" sub-heaps out of
//     the Audio Main heap, each of a configured size, block at sub_8277F0B8 (returning to
//     0x8275E2F8) then the heap at sub_8277DD40 (returning to 0x8275E314); a bank with size
//     0 uses the main heap itself. CZ_AUDIO_BANK_GROW_MB=N makes every sized bank N MiB
//     bigger, falling back to the configured size if the main heap cannot give it — so it
//     needs CZ_AUDIO_MAIN_GROW_MB to cover (banks x N).
//
// All four default to 0: with none set this file is inert and the build is stock (the
// control arm). Each prints one line when it engages, so a run says what it ran with.
#include <cstdio>
#include <cstdlib>

#include "ppc_recomp_shared.h"

extern "C" PPC_FUNC(__imp__sub_825D2688);
extern "C" PPC_FUNC(__imp__sub_82483888);

namespace
{
constexpr uint32_t kTitleMainBlock = 0x1BF16000;
constexpr uint32_t kMainBlockReturn = 0x82496DC4;   // after "bl sub_825D2688"

uint32_t Mb(const char* name, long maxMb)
{
    const char* e = getenv(name);
    if (!e || !*e)
        return 0;
    const long v = strtol(e, nullptr, 10);
    if (v < 0 || v > maxMb)
    {
        fprintf(stderr, "[heapgrow] %s=%s ignored (0..%ld)\n", name, e, maxMb);
        return 0;
    }
    return uint32_t(v) << 20;
}
}   // namespace

uint32_t HeapGrowth_SwTexture()
{
    static const uint32_t g = Mb("CZ_SW_TEXTURE_GROW_MB", 128);
    return g;
}

uint32_t HeapGrowth_AudioBank()
{
    static const uint32_t g = Mb("CZ_AUDIO_BANK_GROW_MB", 64);
    return g;
}

// The title's one big physical block -> heap 0.
PPC_FUNC(sub_825D2688)
{
    static const uint32_t g = Mb("CZ_MAIN_BLOCK_GROW_MB", 64);
    if (!g || ctx.lr != kMainBlockReturn || ctx.r3.u32 != kTitleMainBlock)
    {
        __imp__sub_825D2688(ctx, base);
        return;
    }
    const PPCContext saved = ctx;
    ctx.r3.u64 = kTitleMainBlock + g;
    __imp__sub_825D2688(ctx, base);
    if (ctx.r3.u32)
    {
        fprintf(stderr, "[heapgrow] main block (heap 0): %u -> %u bytes at %08X "
                        "(CZ_MAIN_BLOCK_GROW_MB=%u)\n",
                kTitleMainBlock, kTitleMainBlock + g, ctx.r3.u32, g >> 20);
        return;
    }
    fprintf(stderr, "[heapgrow] main block: %u bytes did not fit in the physical arena; "
                    "taking the title's %u\n", kTitleMainBlock + g, kTitleMainBlock);
    ctx = saved;
    __imp__sub_825D2688(ctx, base);
}

// The Audio Main heap's size, less its fixed 24 MB.
PPC_FUNC(sub_82483888)
{
    __imp__sub_82483888(ctx, base);
    static const uint32_t g = Mb("CZ_AUDIO_MAIN_GROW_MB", 128);
    if (!g)
        return;
    const uint32_t was = ctx.r3.u32;
    ctx.r3.u64 = was + g;
    fprintf(stderr, "[heapgrow] Audio Main heap: %u -> %u bytes (CZ_AUDIO_MAIN_GROW_MB=%u)\n",
            was + 0x1800000, ctx.r3.u32 + 0x1800000, g >> 20);
}
