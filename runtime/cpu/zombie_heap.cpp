// The "Zombie heap" grown by 16 MiB, taken from the streaming world's texture heap, so mods
// can load more zombie types at once.
//
// WHY THIS EXISTS (2026-10-13, for the DR2 CZ/CW modding toolkit, phase 15). The toolkit
// brings Case West's zombie types into Case Zero. The street's load then faulted
// (guest 0x1C in sub_8277E778, "Failed to allocate memory!"): heap 71, the "Zombie heap",
// had 11,968 bytes free and was asked for 33,664. The title makes that heap a constant
// 30,177,280 bytes, and Case Zero's own street leaves only 1.6 MB of it free, so any type
// that is added rather than swapped runs out. A loaded type takes its pack
// (zombies.big/<type>.big) unpacked, plus ~5.48 MB the loaded types share (measured with
// gdb over nine types; the toolkit's docs/formats.md §Engine memory: the heaps).
//
// WHERE THE MEMORY COMES FROM. The title takes one fixed 448 MiB block at boot
// (sub_82496D98) and carves every heap out of it, so the Zombie heap cannot simply ask
// for more: the main heap had 8.4 MB free at the fault. The streaming world's texture
// heap ("SW Texture heap", 200,653,824 bytes, sized by sub_82483860, made once at boot
// by sub_8242E0xx before any level) had 46 MB free after the street loaded. This file
// takes GROWTH bytes off that heap and gives them to the Zombie heap:
//   * sub_82483860 (the texture heap's size, one caller) returns GROWTH less;
//   * the Zombie heap's two calls in sub_825B7548 (re-run for every level) ask GROWTH
//     more: its block from heap 0 (sub_8277F0B8, return address 0x825B76BC) and the
//     heap made in that block (sub_8277DD40, return address 0x825B76D4). Both are
//     generic functions, so only a call from there carrying the title's 30,177,280 is
//     touched (the 20 MB mode, a flag at 0x82A57BE0, is left alone).
// The Zombie heap grows only if the texture heap really shrank first (the block must
// come from somewhere), and the two calls grow together or not at all.
//
// Default: 16 MiB. CZ_NO_ZOMBIE_HEAP_GROWTH=1 keeps the title's sizes (the control).
// CZ_ZOMBIE_HEAP_GROWTH_MB=N picks another amount (1..64). Logs one line per change.
#include <cstdio>
#include <cstdlib>

#include "ppc_recomp_shared.h"

extern "C" PPC_FUNC(__imp__sub_82483860);
extern "C" PPC_FUNC(__imp__sub_8277F0B8);
extern "C" PPC_FUNC(__imp__sub_8277DD40);

namespace
{
constexpr uint32_t kTitleZombieHeap = 30177280;     // 0x01E19000 - 0x151800, sub_825B7548
constexpr uint32_t kBlockReturn = 0x825B76BC;       // after "bl sub_8277F0B8" (its block)
constexpr uint32_t kHeapReturn = 0x825B76D4;        // after "bl sub_8277DD40" (the heap)
constexpr uint32_t kTextureFloor = 64u << 20;       // refuse to shrink a heap smaller than this

uint32_t Growth()
{
    static const uint32_t g = [] {
        if (getenv("CZ_NO_ZOMBIE_HEAP_GROWTH"))
            return 0u;
        if (const char* e = getenv("CZ_ZOMBIE_HEAP_GROWTH_MB"))
        {
            const long v = strtol(e, nullptr, 10);
            if (v >= 1 && v <= 64)
                return uint32_t(v) << 20;
            fprintf(stderr, "[zombieheap] CZ_ZOMBIE_HEAP_GROWTH_MB=%s ignored (1..64)\n", e);
        }
        return 16u << 20;
    }();
    return g;
}

bool g_textureShrunk = false;   // the bytes the Zombie heap takes were freed
bool g_blockGrown = false;      // this level's Zombie heap block was asked bigger
}   // namespace

uint32_t HeapGrowth_SwTexture();   // heap_growth.cpp
uint32_t HeapGrowth_AudioBank();
void SwTextureShare(PPCContext& ctx);

// The SW Texture heap's size: GROWTH less, then CZ_SW_TEXTURE_GROW_MB more.
PPC_FUNC(sub_82483860)
{
    __imp__sub_82483860(ctx, base);
    SwTextureShare(ctx);
    if (const uint32_t extra = HeapGrowth_SwTexture())
    {
        const uint32_t was = ctx.r3.u32;
        ctx.r3.u64 = was + extra;
        fprintf(stderr, "[heapgrow] SW Texture heap: %u -> %u bytes (CZ_SW_TEXTURE_GROW_MB=%u)\n",
                was, ctx.r3.u32, extra >> 20);
    }
}

void SwTextureShare(PPCContext& ctx)
{
    const uint32_t g = Growth();
    if (!g)
        return;
    const uint32_t was = ctx.r3.u32;
    if (was < kTextureFloor + g)
    {
        fprintf(stderr, "[zombieheap] NOT shrinking the SW Texture heap: %u bytes is too small "
                        "(image moved?); the Zombie heap keeps its size\n", was);
        return;
    }
    ctx.r3.u64 = was - g;
    g_textureShrunk = true;
    fprintf(stderr, "[zombieheap] SW Texture heap: %u -> %u bytes (%u MiB to the Zombie heap; "
                    "CZ_NO_ZOMBIE_HEAP_GROWTH=1 keeps both)\n", was, ctx.r3.u32, g >> 20);
}

// The audio store's bank sub-heaps (heap_growth.cpp): block from the Audio Main heap,
// then the heap in it, both CZ_AUDIO_BANK_GROW_MB bigger or neither.
constexpr uint32_t kBankBlockReturn = 0x8275E2F8;
constexpr uint32_t kBankHeapReturn = 0x8275E314;
thread_local bool t_bankGrown = false;

// The Zombie heap's block, from heap 0 (and the audio banks' blocks).
PPC_FUNC(sub_8277F0B8)
{
    if (ctx.lr == kBankBlockReturn)
    {
        t_bankGrown = false;
        const uint32_t g = HeapGrowth_AudioBank();
        if (!g || !ctx.r3.u32)
        {
            __imp__sub_8277F0B8(ctx, base);
            return;
        }
        const PPCContext saved = ctx;
        const uint32_t want = ctx.r3.u32;
        ctx.r3.u64 = want + g;
        __imp__sub_8277F0B8(ctx, base);
        if (ctx.r3.u32)
        {
            t_bankGrown = true;
            return;
        }
        fprintf(stderr, "[heapgrow] audio bank block %u + %u MiB did not fit in the Audio Main "
                        "heap; keeping %u (raise CZ_AUDIO_MAIN_GROW_MB)\n", want, g >> 20, want);
        ctx = saved;
        __imp__sub_8277F0B8(ctx, base);
        return;
    }
    const bool zombieBlock = ctx.lr == kBlockReturn;   // read now: the callee changes lr
    if (zombieBlock)
    {
        g_blockGrown = false;
        if (g_textureShrunk && ctx.r3.u32 == kTitleZombieHeap)
        {
            ctx.r3.u64 = kTitleZombieHeap + Growth();
            g_blockGrown = true;
        }
    }
    __imp__sub_8277F0B8(ctx, base);
    if (zombieBlock && g_blockGrown && ctx.r3.u32 == 0)
        fprintf(stderr, "[zombieheap] the grown Zombie heap block (%u bytes) did not fit in "
                        "heap 0\n", kTitleZombieHeap + Growth());
}

void HeapCensus_OnCreate(uint8_t* base, uint32_t lr, uint32_t heapBase, uint32_t size,
                         uint32_t name, uint32_t parent, int32_t id);   // heap_census.cpp

// The Zombie heap itself, in that block: the same size as the block.
PPC_FUNC(sub_8277DD40)
{
    const uint32_t lr = ctx.lr;
    const uint32_t a3 = ctx.r3.u32, a5 = ctx.r5.u32, a6 = ctx.r6.u32;
    if (ctx.lr == kHeapReturn && g_blockGrown && ctx.r4.u32 == kTitleZombieHeap)
    {
        ctx.r4.u64 = kTitleZombieHeap + Growth();
        static bool logged = false;
        if (!logged)
            fprintf(stderr, "[zombieheap] Zombie heap: %u -> %u bytes (mods can load more "
                            "zombie types at once)\n", kTitleZombieHeap, ctx.r4.u32);
        logged = true;
    }
    if (ctx.lr == kBankHeapReturn && t_bankGrown)
    {
        t_bankGrown = false;
        const uint32_t was = ctx.r4.u32;
        ctx.r4.u64 = was + HeapGrowth_AudioBank();
        fprintf(stderr, "[heapgrow] audio bank heap: %u -> %u bytes (CZ_AUDIO_BANK_GROW_MB=%u)\n",
                was, ctx.r4.u32, HeapGrowth_AudioBank() >> 20);
    }
    const uint32_t a4 = ctx.r4.u32;
    __imp__sub_8277DD40(ctx, base);
    HeapCensus_OnCreate(base, lr, a3, a4, a5, a6, int32_t(ctx.r3.u32));
}
