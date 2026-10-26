// CZ_NO_OCCLUDERS=1: load every zone with zero occluders — the test of whether store
// shelves that vanish a few metres from Chuck are OCCLUSION-culled rather than unloaded.
//
// WHY THIS EXISTS (2026-10-25, the general store). With the heaps grown and every item's
// LoadDistance x3 (cpu/item_distance.cpp), shelves in the general store still vanish
// from a few metres away, and Xenia does the same. Distance and memory are ruled out
// (the item vote sub_82174EE8 scores them well inside range; the streamer never left
// level 1). What is left is visibility: each zone carries hand-placed occluder volumes,
// read by the zone loader sub_82229AF0(zone, ...) from the zone file's "NumOccluder" /
// "OccluderArray2" into zone+0x214 (count) / zone+0x218 (array). A count of 0 is a
// state the title already handles (the loader itself writes 0 when the array is
// missing), so zeroing it after the load turns occlusion culling off for that zone.
//
// Every zone's occluder count is printed when this or CZ_OCCLUDER_TRACE=1 is set.
#include <cstdio>
#include <cstdlib>

#include "ppc_recomp_shared.h"

extern "C" PPC_FUNC(__imp__sub_82229AF0);

PPC_FUNC(sub_82229AF0)
{
    const uint32_t zone = ctx.r3.u32;
    __imp__sub_82229AF0(ctx, base);
    static const bool off = getenv("CZ_NO_OCCLUDERS") != nullptr;
    static const bool trace = off || getenv("CZ_OCCLUDER_TRACE") != nullptr;
    if (!trace || !zone)
        return;
    const uint32_t n = PPC_LOAD_U32(zone + 0x214);
    fprintf(stderr, "[occluders] zone %08X loaded with %u occluder(s)%s\n", zone, n,
            off && n ? " — zeroed (CZ_NO_OCCLUDERS)" : "");
    if (off)
        PPC_STORE_U32(zone + 0x214, 0);
}
