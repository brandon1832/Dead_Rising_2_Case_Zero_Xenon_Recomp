// CZ_VOLUME_TRACE=1: every static-geometry volume's detailed/far switch, named, with the
// distance and the switch distance at that moment — the store-shelf instrument.
//
// WHY THIS EXISTS (2026-10-25, the general store). Shelf contents in the general store
// vanish a few metres from Chuck (Xenia too), and memory, item LoadDistance (x3), an item
// cap and occlusion (CZ_NO_OCCLUDERS) are all ruled out. Each zone's static geometry is
// a list of volumes (cZone +0x120 count, 0xD0-byte records at [+0x124]: name at +0x20,
// sphere centre/radius at +0x80..+0x8C, skip bit at +0x90, load state at +0xA0/+0xA4,
// switch distance at +0xA8), and sub_82175040(zone, index) is the per-volume vote the
// zone async loader asks: 1 = use the FAR version, 0 = the detailed one. It returns 1 at
// once when the shipped-off tunable ForceLODTexForStreamingWorld (0x82A57BD7) is set, 0
// at once when the volume's skip bit is set, and otherwise compares
//     |camera - centre| - camera radius - volume radius   against   the switch distance
// boosted by WORLD DETAIL (cpu/world_lod.cpp). If a shelf's products are part of its
// DETAILED model, an empty shelf is that shelf's far version, and this trace names it.
//
// It logs a line whenever a volume's answer or load state changes, never per frame.
// CZ_SW_FILE_LOG=1 also switches on the title's own "[SW-FileEvent] (time) ..." log
// (EnableStreamingWorldFileEventLog, 0x82A57BD9; printed through CZ_GUEST_LOG=1).
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <utility>

#include "ppc_recomp_shared.h"

extern "C" PPC_FUNC(__imp__sub_82175040);

namespace
{
float F(uint8_t* base, uint32_t a)
{
    const uint32_t b = PPC_LOAD_U32(a);
    float f;
    memcpy(&f, &b, 4);
    return f;
}
double Now()
{
    static const auto t0 = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}
struct State { uint32_t vote, a0, a4; };
}   // namespace

PPC_FUNC(sub_82175040)
{
    static const bool trace = getenv("CZ_VOLUME_TRACE") != nullptr;
    static const bool swlog = getenv("CZ_SW_FILE_LOG") != nullptr;
    if (swlog)
        PPC_STORE_U8(0x82A57BD9, 1);
    const uint32_t zone = ctx.r3.u32, idx = ctx.r4.u32;
    // DETAIL FLOOR (CZ_DETAIL_MIN_SWITCH=M, metres). The store's products are volumes
    // with a 2 m switch distance (z08_orangejuice, z08_motoroil, z08_ketchup ...) and
    // shop interiors use 2-5 m, so even WORLD DETAIL 4.0 flips them to their far version
    // (the empty shelf) 8 m away. Any volume authored below M is raised to M before the
    // vote reads it; WORLD DETAIL then multiplies as usual. Written into the volume once
    // (idempotent, so a concurrent voter seeing it early is harmless); a zone reload
    // re-reads the title's value and the next vote raises it again.
    static const float minSwitch = [] {
        const char* e = getenv("CZ_DETAIL_MIN_SWITCH");
        const float v = e ? float(atof(e)) : 0.0f;
        if (v > 0.0f)
            fprintf(stderr, "[volume] detail floor: every scenery switch distance below "
                            "%.1f m is raised to it (CZ_DETAIL_MIN_SWITCH)\n", v);
        return v > 0.0f && v <= 100.0f ? v : 0.0f;
    }();
    if (minSwitch > 0.0f && zone && idx < PPC_LOAD_U32(zone + 0x120))
    {
        const uint32_t vol = PPC_LOAD_U32(zone + 0x124) + idx * 0xD0;
        if (F(base, vol + 0xA8) < minSwitch)
        {
            uint32_t b;
            memcpy(&b, &minSwitch, 4);
            PPC_STORE_U32(vol + 0xA8, b);
            static std::atomic<uint32_t> raised{0};
            const uint32_t n = ++raised;
            if (n == 1 || n % 100 == 0)
                fprintf(stderr, "[volume] detail floor: %u scenery switch distances raised "
                                "to %.1f m so far\n", n, minSwitch);
        }
    }
    __imp__sub_82175040(ctx, base);
    if (!trace || !zone || idx >= 4096)
        return;
    const uint32_t vote = ctx.r3.u32;
    const uint32_t vol = PPC_LOAD_U32(zone + 0x124) + idx * 0xD0;
    const State now{vote, PPC_LOAD_U32(vol + 0xA0), PPC_LOAD_U32(vol + 0xA4)};
    static std::mutex mu;
    static std::map<std::pair<uint32_t, uint32_t>, State> last;
    std::lock_guard<std::mutex> lk(mu);
    auto [it, fresh] = last.try_emplace({zone, idx}, now);
    if (!fresh && it->second.vote == now.vote && it->second.a0 == now.a0 &&
        it->second.a4 == now.a4)
        return;
    it->second = now;
    char name[33] = {};
    for (int i = 0; i < 32; ++i)
    {
        const char c = char(base[vol + 0x20 + i]);
        if (c < 0x20 || c > 0x7E)
            break;
        name[i] = c;
    }
    const uint32_t g = PPC_LOAD_U32(0x82A46294);
    const float cx = F(base, g + 0x40), cy = F(base, g + 0x44), cz = F(base, g + 0x48);
    const float vx = F(base, vol + 0x80), vy = F(base, vol + 0x84), vz = F(base, vol + 0x88),
                vr = F(base, vol + 0x8C);
    const float d = std::sqrt((cx - vx) * (cx - vx) + (cy - vy) * (cy - vy) +
                              (cz - vz) * (cz - vz));
    fprintf(stderr, "[volume] t=%.2f zone %08X #%u '%s' %s  dist-to-centre %.1f radius %.1f "
                    "switch %.1f skip %u  state A0=%08X A4=%08X  camera (%.1f, %.1f, %.1f)%s\n",
            Now(), zone, idx, name, vote ? "FAR" : "DETAILED", d, vr, F(base, vol + 0xA8),
            PPC_LOAD_U32(vol + 0x90) & 1, now.a0, now.a4, cx, cy, cz, fresh ? " (first)" : "");
}
