// The renderer's "Shader Constant Sets" pool, grown so mods can bring pixel shaders.
//
// WHY THIS EXISTS (2026-10-12, for the DR2 CZ/CW modding toolkit, phase 15). A model
// brought from Case West draws with Case West's own pixel shaders, which the toolkit
// adds to Case Zero's shader banks. With ~29 or more added shaders the title asserts
//   INVALID_HANDLE != shader_constant_set_handle && "Shader constant set overflow."
// (shaderconstantsetstack.cpp; sub_827B0BB8 -> the free list sub_82195EF8 returns -1).
// The renderer keeps ONE constant set per loaded pixel shader in a fixed pool, and
// Case Zero ships 1,280 pixel shaders against a pool of 1,300.
//
// WHERE THE SIZE COMES FROM. The renderer's pool builder sub_827BE658 reads the size at
// 0x827BEF4C (`lwz r4, -10252(r11)` = 0x829ED7F4) and builds the pool kept at renderer
// +3492 (sub_827BC868). Both arrays are allocated FROM that count (sub_827AC4E8:
// count*12+4 bytes of sets; sub_82154788: count*8 bytes of free list), so a larger
// value is a larger pool, not an overrun. The image's data says 384 there, but the boot
// budget function sub_825D37C8 (called unconditionally from sub_825D6358 at 0x825D63CC,
// before the renderer is made) overwrites it with 1300; so the value has to be raised
// AFTER that, which is why this hooks the pool builder itself and raises the size just
// before it reads it. Handles are stored as u32 everywhere except one u16 store
// (0x827D933C), so any size up to 65535 fits.
//
// Default: 4096 sets (~80 KB of guest heap more). CZ_NO_SHADER_POOL_GROWTH=1 keeps the
// title's own size (the control). CZ_SHADER_POOL=N picks another size (1300..65535).
// Logs one line when it raises the size. The write is refused, loudly, when the word
// there is not the title's 1300 at that moment: the one check that catches a moved image.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ppc_recomp_shared.h"

extern "C" PPC_FUNC(__imp__sub_827BE658);

namespace
{
constexpr uint32_t kPixelConstantSetPoolSize = 0x829ED7F4;   // read at 0x827BEF4C
constexpr uint32_t kTitleSize = 1300;        // what sub_825D37C8 stored
constexpr uint32_t kDefaultGrownSize = 4096;

uint32_t GrownSize()
{
    if (getenv("CZ_NO_SHADER_POOL_GROWTH"))
        return 0;
    if (const char* e = getenv("CZ_SHADER_POOL"))
    {
        const long v = strtol(e, nullptr, 10);
        if (v >= 1300 && v <= 65535)
            return uint32_t(v);
        fprintf(stderr, "[shaderpool] CZ_SHADER_POOL=%s ignored (1300..65535)\n", e);
    }
    return kDefaultGrownSize;
}
}   // namespace

// The renderer's pool builder: raise the pixel shader constant set count, then build.
PPC_FUNC(sub_827BE658)
{
    const uint32_t grown = GrownSize();
    if (grown)
    {
        uint32_t be;
        memcpy(&be, base + kPixelConstantSetPoolSize, 4);
        const uint32_t was = __builtin_bswap32(be);
        if (was == grown)
            ;   // a second build of the pools: already grown
        else if (was != kTitleSize)
        {
            static bool said = false;
            if (!said)
                fprintf(stderr,
                        "[shaderpool] NOT growing: 0x%08X holds %u, expected %u (image "
                        "moved?)\n", kPixelConstantSetPoolSize, was, kTitleSize);
            said = true;
        }
        else if (was < grown)
        {
            be = __builtin_bswap32(grown);
            memcpy(base + kPixelConstantSetPoolSize, &be, 4);
            fprintf(stderr,
                    "[shaderpool] shader constant sets: %u -> %u (mods can bring pixel "
                    "shaders; CZ_NO_SHADER_POOL_GROWTH=1 keeps %u)\n",
                    was, grown, was);
        }
    }
    __imp__sub_827BE658(ctx, base);
}
