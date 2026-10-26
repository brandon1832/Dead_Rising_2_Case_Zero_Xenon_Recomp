// ITEM DISTANCE: every item's LoadDistance (items.txt) scaled by one factor.
//
// WHY THIS EXISTS (2026-10-25, the "more streaming and audio memory" fork). With the
// streaming and audio heaps grown (cpu/heap_growth.cpp) the streamer no longer runs out
// of memory at WORLD DETAIL 4.0 / ZOMBIE DETAIL 5.0 (measured: 0 of 31,185 checks below
// its 4 MB threshold), and what still pops in is the title's own data: every item def in
// data/datafile.big:items.txt carries a `LoadDistance` in metres (mailbox 35, orange
// juice 20, most items 15-50; 76 defs say 10000 = always), and an item exists only
// inside it. On a real 360 and in Xenia the same.
//
// WHERE IT IS READ. The item def registers the property through the universal binder
// (sub_82375518, name 0x82046A60 "LoadDistance", field this+0x3C; sub_8239C1F8). A
// hardware read-watchpoint on two defs' fields over a headless DebugJump run (boot,
// level load, outdoor play) caught exactly ONE reader in the title: sub_823E24D0, a
// virtual getter (no direct callers) that returns f1 = def+0x3C, or a global default
// when the def leaves it 0. Its callers came from the level's prop placement
// (sub_8243C378 -> sub_8221EF30) and the item spawner (sub_8247B428 -> sub_82495CB8).
// So the getter is the one place to scale: nothing in guest memory is rewritten, and
// whatever the title derives from the distance (spawn and despawn alike) moves with it.
//
// CZ_ITEM_DISTANCE=F (1.0..4.0) engages it; unset or 1 = stock (the control arm).
// Results >= 1000 m (the "always loaded" class) are left alone.
// CZ_ITEM_DIST_TRACE=1 prints each distinct (caller, distance) once.
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <set>
#include <utility>

#include "ppc_recomp_shared.h"

extern "C" PPC_FUNC(__imp__sub_823E24D0);

namespace
{
float Factor()
{
    static const float f = [] {
        const char* e = getenv("CZ_ITEM_DISTANCE");
        if (!e || !*e)
            return 1.0f;
        const float v = float(atof(e));
        if (v >= 1.0f && v <= 4.0f)
        {
            if (v != 1.0f)
                fprintf(stderr, "[itemdist] item LoadDistance x%.2f (CZ_ITEM_DISTANCE; values "
                                ">= 1000 m untouched)\n", v);
            return v;
        }
        fprintf(stderr, "[itemdist] CZ_ITEM_DISTANCE=%s ignored (1.0..4.0)\n", e);
        return 1.0f;
    }();
    return f;
}
}   // namespace

PPC_FUNC(sub_823E24D0)
{
    const uint32_t lr = ctx.lr;
    __imp__sub_823E24D0(ctx, base);
    const float in = float(ctx.f1.f64);
    const float f = Factor();
    if (f != 1.0f && in > 0.0f && in < 1000.0f)
        ctx.f1.f64 = double(in * f);
    static const bool trace = getenv("CZ_ITEM_DIST_TRACE") != nullptr;
    if (trace)
    {
        static std::mutex mu;
        static std::set<std::pair<uint32_t, float>> seen;
        std::lock_guard<std::mutex> lk(mu);
        if (seen.size() < 200 && seen.emplace(lr, in).second)
            fprintf(stderr, "[itemdist] caller %08X LoadDistance %.1f -> %.1f\n", lr, in,
                    float(ctx.f1.f64));
    }
}
