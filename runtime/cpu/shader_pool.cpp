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
// A second pool beside it (0x829ED7EC, also 1300 after boot) is grown the same way (see
// kPools). Logs one line per pool raised. A write is refused, loudly, when the word there
// is not the title's 1300 at that moment: the one check that catches a moved image.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ppc_recomp_shared.h"

extern "C" PPC_FUNC(__imp__sub_827BE658);

namespace
{
// The two pools sub_825D37C8 sizes 1300, each read by the builder below. The second one
// is grown too because Case West's matching pool (its 0x82A00D2C, 1340) ran out with
// 14 brought pixel shaders (a failed allocation, a fault reading 0x1C; Case West gotcha
// 332); here 45 loaded without it, but the two ports stay the same shape.
struct PoolWord
{
    uint32_t address;
    uint32_t title;     // what sub_825D37C8 stored
    const char* what;
};
constexpr PoolWord kPools[] = {
    {0x829ED7F4, 1300, "shader constant sets (read at 0x827BEF4C, renderer +3492)"},
    {0x829ED7EC, 1300, "the pool read at 0x827BEEB4 (renderer +3480)"},
};
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

void Grow(uint8_t* base, const PoolWord& p, uint32_t grown)
{
    uint32_t be;
    memcpy(&be, base + p.address, 4);
    const uint32_t was = __builtin_bswap32(be);
    if (was == grown)
        return;   // a second build of the pools: already grown
    if (was != p.title)
    {
        fprintf(stderr, "[shaderpool] NOT growing %s: 0x%08X holds %u, expected %u (image "
                        "moved?)\n", p.what, p.address, was, p.title);
        return;
    }
    be = __builtin_bswap32(grown);
    memcpy(base + p.address, &be, 4);
    fprintf(stderr, "[shaderpool] %s: %u -> %u (mods can bring pixel shaders; "
                    "CZ_NO_SHADER_POOL_GROWTH=1 keeps %u)\n", p.what, was, grown, was);
}
}   // namespace

// The renderer's pool builder: raise the pools' counts, then build.
PPC_FUNC(sub_827BE658)
{
    static bool done = false;
    if (const uint32_t grown = GrownSize(); grown && !done)
        for (const PoolWord& p : kPools)
            Grow(base, p, grown);
    done = true;
    __imp__sub_827BE658(ctx, base);
}
