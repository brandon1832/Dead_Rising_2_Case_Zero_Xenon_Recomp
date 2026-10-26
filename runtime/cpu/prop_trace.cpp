// CZ_PROP_TRACE=1: every dynamic prop ID allocated and freed, with the item's name where it
// can be found. A diagnostic for "the general store's items vanish at the far wall": it
// tells a prop that is DESPAWNED (its ID freed) from one that is only hidden.
//
//   sub_821F0FB0(mgr, type, ...) -> id   AllocateInstanceId ("AllocateInstanceId -
//                                         Duplicate prop ID..."), callers: the level's
//                                         item placement sub_8221EF30, sub_821F1280,
//                                         sub_82271150
//   sub_821A3290(mgr, prop)              PropIDFreedCallback ("PropIDFreedCallback - Ran
//                                         out of room..."), prop+0x14 = the instance ID
//                                         (type = bits 13..15, index = low 13 bits)
// The item def for a prop is found by matching the prop's words against the defs seen
// registering LoadDistance (item_distance.cpp), and the def's name is the first string
// one of its words points at.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "ppc_recomp_shared.h"

extern "C" PPC_FUNC(__imp__sub_821F0FB0);
extern "C" PPC_FUNC(__imp__sub_821A3290);

namespace
{
bool On()
{
    static const bool on = getenv("CZ_PROP_TRACE") != nullptr;
    return on;
}
double Now()
{
    static const auto t0 = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}
std::mutex g_mu;
std::unordered_set<uint32_t> g_defs;
std::unordered_map<uint32_t, std::string> g_names;

bool Str(uint8_t* base, uint32_t p, std::string& out)
{
    if (p < 0x82000000u || p >= 0xC0000000u)
        return false;
    const char* s = reinterpret_cast<const char*>(base + p);
    size_t n = 0;
    while (n < 48 && s[n] >= 0x21 && s[n] < 0x7F)
        ++n;
    if (n < 4 || s[n] != 0)
        return false;
    out.assign(s, n);
    return true;
}

std::string DefName(uint8_t* base, uint32_t def)
{
    auto it = g_names.find(def);
    if (it != g_names.end())
        return it->second;
    std::string s = "?";
    for (uint32_t off = 4; off < 0x200; off += 4)
    {
        std::string t;
        if (Str(base, PPC_LOAD_U32(def + off), t))
        {
            s = t + "@+" + std::to_string(off);
            break;
        }
    }
    g_names[def] = s;
    return s;
}

std::string PropName(uint8_t* base, uint32_t prop, uint32_t& defOff)
{
    defOff = 0;
    for (uint32_t off = 0; off < 0x400; off += 4)
    {
        const uint32_t w = PPC_LOAD_U32(prop + off);
        if (g_defs.count(w))
        {
            defOff = off;
            return DefName(base, w);
        }
        // one level of indirection
        if (w >= 0xA0000000u && w < 0xC0000000u)
            for (uint32_t o2 = 0; o2 < 0x40; o2 += 4)
            {
                const uint32_t w2 = PPC_LOAD_U32(w + o2);
                if (g_defs.count(w2))
                {
                    defOff = 0x10000 * off + o2;
                    return DefName(base, w2);
                }
            }
    }
    return "";
}
}   // namespace

void PropTrace_OnDef(uint32_t def)
{
    if (!On())
        return;
    std::lock_guard<std::mutex> lk(g_mu);
    g_defs.insert(def);
}

PPC_FUNC(sub_821F0FB0)
{
    const uint32_t lr = ctx.lr, type = ctx.r4.u32;
    __imp__sub_821F0FB0(ctx, base);
    if (!On())
        return;
    std::lock_guard<std::mutex> lk(g_mu);
    static uint64_t n = 0;
    if (++n <= 4000)
        fprintf(stderr, "[proptrace] t=%.2f ALLOC type %u -> id %08X (caller %08X)\n", Now(),
                type, ctx.r3.u32, lr);
}

PPC_FUNC(sub_821A3290)
{
    const uint32_t prop = ctx.r4.u32;
    if (On() && prop)
    {
        std::lock_guard<std::mutex> lk(g_mu);
        const uint32_t id = PPC_LOAD_U32(prop + 0x14);
        uint32_t defOff = 0;
        const std::string name = PropName(base, prop, defOff);
        static uint64_t n = 0;
        if (++n <= 4000)
            fprintf(stderr, "[proptrace] t=%.2f FREE id %08X (type %u index %u) prop %08X "
                            "vtable %08X item '%s' (def at +%X)\n",
                    Now(), id, (id >> 13) & 7, id & 0x1FFF, prop, PPC_LOAD_U32(prop),
                    name.c_str(), defOff);
    }
    __imp__sub_821A3290(ctx, base);
}
