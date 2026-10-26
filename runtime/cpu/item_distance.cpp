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
#include <unordered_map>
#include <vector>
#include <cctype>
#include <string>
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

// The item def's name, for CZ_ITEM_DIST_TRACE: the first word of the def that points at
// a short printable string (found by census, printed with its offset so it can be pinned).
static std::string DefName(uint8_t* base, uint32_t def, uint32_t& off)
{
    for (off = 4; off < 0x400; off += 4)
    {
        const uint32_t p = PPC_LOAD_U32(def + off);
        if (p < 0x82000000u || p >= 0xC0000000u)
            continue;
        const char* s = reinterpret_cast<const char*>(base + p);
        size_t n = 0;
        while (n < 40 && s[n] > 0x20 && s[n] < 0x7F)
            ++n;
        if (n >= 3 && s[n] == 0)
            return std::string(s, n);
    }
    off = 0;
    return "?";
}

// PER-ITEM OVERRIDES: CZ_ITEM_DISTANCE_OVERRIDE="mailbox=160,newspaperbox=150" sets an
// item's load distance to that many metres outright (instead of x CZ_ITEM_DISTANCE). The
// key is a case-insensitive substring of the def's model path (+0x60, e.g.
// "data/models/weapons/mailbox") or its short name (+0x88, e.g. "w_garbagecan").
// WHY: the operator's mailboxes (LoadDistance 35) still fade in close at x2 while trash
// cans (110) do not; the fade follows the load distance (trash cans were see-through at
// x1.5 and fixed at x2), so the short-ranged item needs more range of its own.
namespace
{
struct Override { std::string key; float metres; };
const std::vector<Override>& Overrides()
{
    static const std::vector<Override> v = [] {
        std::vector<Override> out;
        const char* e = getenv("CZ_ITEM_DISTANCE_OVERRIDE");
        if (!e)
            return out;
        std::string s(e);
        for (size_t pos = 0; pos <= s.size();)
        {
            const size_t comma = s.find(',', pos);
            const std::string item = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
            pos = comma == std::string::npos ? s.size() + 1 : comma + 1;
            const size_t eq = item.find('=');
            if (eq == std::string::npos || eq == 0)
                continue;
            std::string key = item.substr(0, eq);
            for (char& c : key)
                c = char(tolower((unsigned char)c));
            const float m = float(atof(item.c_str() + eq + 1));
            if (m > 0.0f && m < 1000.0f)
            {
                out.push_back({key, m});
                fprintf(stderr, "[itemdist] override: items matching '%s' load at %.0f m\n",
                        key.c_str(), m);
            }
        }
        return out;
    }();
    return v;
}

std::string Lower(uint8_t* base, uint32_t p)
{
    std::string out;
    if (p < 0x82000000u || p >= 0xC0000000u)
        return out;
    const char* s = reinterpret_cast<const char*>(base + p);
    for (size_t n = 0; n < 96 && s[n] > 0x20 && s[n] < 0x7F; ++n)
        out += char(tolower((unsigned char)s[n]));
    return out;
}

// -1 = no override. Cached per def (defs live for the whole session).
float OverrideFor(uint8_t* base, uint32_t def)
{
    const auto& ov = Overrides();
    if (ov.empty() || !def)
        return -1.0f;
    static std::mutex mu;
    static std::unordered_map<uint32_t, float> cache;
    std::lock_guard<std::mutex> lk(mu);
    auto it = cache.find(def);
    if (it != cache.end())
        return it->second;
    const std::string path = Lower(base, PPC_LOAD_U32(def + 0x60));
    const std::string name = Lower(base, PPC_LOAD_U32(def + 0x88));
    float r = -1.0f;
    for (const Override& o : ov)
        if ((!path.empty() && path.find(o.key) != std::string::npos) ||
            (!name.empty() && name.find(o.key) != std::string::npos))
        {
            r = o.metres;
            fprintf(stderr, "[itemdist] override '%s' -> %.0f m for '%s'\n", o.key.c_str(), r,
                    !path.empty() ? path.c_str() : name.c_str());
            break;
        }
    cache[def] = r;
    return r;
}
}   // namespace

PPC_FUNC(sub_823E24D0)
{
    const uint32_t lr = ctx.lr;
    const uint32_t def = ctx.r3.u32;
    __imp__sub_823E24D0(ctx, base);
    const float in = float(ctx.f1.f64);
    const float f = Factor();
    const float ov = OverrideFor(base, def);
    if (ov > 0.0f && in > 0.0f && in < 1000.0f)
        ctx.f1.f64 = double(ov);
    else if (f != 1.0f && in > 0.0f && in < 1000.0f)
        ctx.f1.f64 = double(in * f);
    static const bool trace = getenv("CZ_ITEM_DIST_TRACE") != nullptr;
    if (trace)
    {
        static std::mutex mu;
        static std::set<std::pair<uint32_t, float>> seen;
        std::lock_guard<std::mutex> lk(mu);
        if (seen.size() < 200 && seen.emplace(lr, in).second)
        {
            uint32_t off = 0;
            const std::string name = DefName(base, def, off);
            fprintf(stderr, "[itemdist] caller %08X def %08X '%s' (+%X) LoadDistance %.1f -> "
                            "%.1f\n", lr, def, name.c_str(), off, in, float(ctx.f1.f64));
        }
    }
}
