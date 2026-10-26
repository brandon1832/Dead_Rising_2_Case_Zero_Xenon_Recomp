// The title's named heaps, measured live: every heap's size, total free and largest free
// block, the heap the world streamer judges "out of memory" by, the audio sample heap's
// failures, and what is left of the physical arena outside the title's 447 MB block.
//
// WHY THIS EXISTS (2026-10-25, the "more streaming and audio memory" fork). At WORLD
// DETAIL 4.0 / ZOMBIE DETAIL 5.0 the streamer prints "WE RAN OUT OF MEMORY!!!" and holds
// zones back (pop-in), and the audio library prints "Out of Audio Sample Memory" (missing
// sounds). Growing those heaps means taking bytes from somewhere else, and the only honest
// way to choose where is to measure where the slack is, at the settings that fail. This
// is that measurement. It changes nothing in the guest: it only READS the heap tables and
// calls the title's own two query functions.
//
// WHAT IT READS (all derived from the title's own code, tools/gdis.py):
//   * sub_8277DD40(base, size, name, parent) makes heap `id` (returned in r3, 1..127) and
//     fills four tables indexed by id*4: the live header at 0x82A8A070 (non-zero = live),
//     the size at 0x82A73ED0, the base at 0x82A8AA98, the name pointer at 0x829ECD28.
//   * sub_8277C288(id) = total free bytes, sub_8277C1A8(id, align) = largest free block.
//     Both take the heap manager's own lock, so calling them from a guest thread is safe.
//   * sub_8222BD70(streamer) is the streamer's memory check: it reads the heap id at
//     [*(0x82A46294) + 0x34F48], stores total free at +0x0C, largest free at +0x10 and
//     the level at +0x08 (1 normal; 2 below 0x420000 largest free; 3 below 0x320000).
//   * sub_8272C298 is the audio sample allocation. Its check is the call to sub_8277C1A8
//     returning to 0x8272C2D4 with the heap id in r3, compared against the size needed,
//     which is in the caller's r31 when sub_8277C1A8 returns. Below it: the
//     "Out of Audio Sample Memory" message.
//
// Off by default and free when off. CZ_HEAP_CENSUS=1 turns it on (a full census every
// 15 s, CZ_HEAP_CENSUS_S=N for another interval, plus one at every streamer level
// change into 2 or 3 and after audio sample failures, each rate-limited to one per 5 s).
// Every heap creation is logged too, with its caller.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>

#include "ppc_recomp_shared.h"
#include "../kernel/heap.h"

extern "C" PPC_FUNC(__imp__sub_8222BD70);
extern "C" PPC_FUNC(__imp__sub_8277C1A8);
extern "C" PPC_FUNC(__imp__sub_8277C288);

namespace
{
constexpr uint32_t kHeapLive = 0x82A8A070;
constexpr uint32_t kHeapSize = 0x82A73ED0;
constexpr uint32_t kHeapBase = 0x82A8AA98;
constexpr uint32_t kHeapName = 0x829ECD28;
constexpr uint32_t kStreamerGlobal = 0x82A46294;
constexpr uint32_t kStreamerHeapOff = 0x34F48;
constexpr uint32_t kAudioSampleCheckReturn = 0x8272C2D4;
constexpr int kMaxHeaps = 128;

using Clock = std::chrono::steady_clock;
const Clock::time_point g_t0 = Clock::now();
double Now() { return std::chrono::duration<double>(Clock::now() - g_t0).count(); }

bool Enabled()
{
    static const bool on = [] {
        const char* e = getenv("CZ_HEAP_CENSUS");
        return e && *e && strcmp(e, "0") != 0;
    }();
    return on;
}

double Interval()
{
    static const double s = [] {
        if (const char* e = getenv("CZ_HEAP_CENSUS_S"))
        {
            const double v = atof(e);
            if (v >= 1.0)
                return v;
        }
        return 15.0;
    }();
    return s;
}

std::mutex g_mutex;
double g_lastCensus = -1e9;
double g_lastEventCensus = -1e9;
int g_streamerHeap = -1;
uint32_t g_streamerLevel = 0;
uint32_t g_streamerLargest = 0, g_streamerTotal = 0;
uint32_t g_streamerMinLargest = 0xFFFFFFFF;    // since the last census
uint64_t g_levelFrames[4] = {};                // checks at each level since the last census

struct AudioFail { uint32_t count = 0, maxNeed = 0, minLargest = 0xFFFFFFFF; };
AudioFail g_audioFail[kMaxHeaps];
uint32_t g_audioChecks[kMaxHeaps] = {};
uint32_t g_audioMaxNeed[kMaxHeaps] = {};       // the largest request, failed or not
bool g_audioFailPending = false;

std::string HeapName(uint8_t* base, int id)
{
    const uint32_t p = PPC_LOAD_U32(kHeapName + id * 4);
    if (!p || p < 0x82000000 || p >= 0x83000000)
        return "?";
    const char* s = reinterpret_cast<const char*>(base + p);
    return std::string(s, strnlen(s, 64));
}

// The title's own queries, on a copy of the hooking thread's context: the copy keeps the
// caller's stack pointer, so the callee builds its frame below it exactly as a real call
// from that point would.
uint32_t GuestCall1(PPCContext& ctx, uint8_t* base, PPCFunc* fn, uint32_t a, uint32_t b)
{
    PPCContext c = ctx;
    c.r3.u64 = a;
    c.r4.u64 = b;
    fn(c, base);
    return c.r3.u32;
}

void Census(PPCContext& ctx, uint8_t* base, const char* why)
{
    // Caller holds g_mutex.
    const double t = Now();
    fprintf(stderr, "[heapcensus] ==== t=%.1fs (%s) ====\n", t, why);
    fprintf(stderr, "[heapcensus] streamer: heap %d, level %u, largest free %u, total free %u, "
                    "min largest since last %u; checks at level 1/2/3: %llu/%llu/%llu\n",
            g_streamerHeap, g_streamerLevel, g_streamerLargest, g_streamerTotal,
            g_streamerMinLargest == 0xFFFFFFFF ? 0 : g_streamerMinLargest,
            (unsigned long long)g_levelFrames[1], (unsigned long long)g_levelFrames[2],
            (unsigned long long)g_levelFrames[3]);
    uint64_t sumSize = 0, sumFree = 0;
    int n = 0;
    for (int id = 0; id < kMaxHeaps; ++id)
    {
        if (!PPC_LOAD_U32(kHeapLive + id * 4))
            continue;
        const uint32_t size = PPC_LOAD_U32(kHeapSize + id * 4);
        const uint32_t hb = PPC_LOAD_U32(kHeapBase + id * 4);
        const uint32_t total = GuestCall1(ctx, base, __imp__sub_8277C288, id, 0);
        const uint32_t largest = GuestCall1(ctx, base, __imp__sub_8277C1A8, id, 0x10);
        fprintf(stderr, "[heapcensus] heap %3d %-28s size %10u base %08X free %10u "
                        "largest %10u (%5.1f%% free)%s\n",
                id, HeapName(base, id).c_str(), size, hb, total, largest,
                size ? 100.0 * total / size : 0.0, id == g_streamerHeap ? "  <- STREAMER" : "");
        sumSize += size;
        sumFree += total;
        ++n;
    }
    fprintf(stderr, "[heapcensus] %d live heaps, %llu bytes in heaps, %llu free (sizes nest: "
                    "sub-heaps are counted inside their parent too)\n",
            n, (unsigned long long)sumSize, (unsigned long long)sumFree);
    for (int id = 0; id < kMaxHeaps; ++id)
    {
        if (!g_audioChecks[id] && !g_audioFail[id].count)
            continue;
        fprintf(stderr, "[heapcensus] audio samples on heap %d %s: %u allocations checked, "
                        "largest asked %u; %u FAILED (largest failed ask %u, largest free at a "
                        "failure %u)\n",
                id, HeapName(base, id).c_str(), g_audioChecks[id], g_audioMaxNeed[id],
                g_audioFail[id].count, g_audioFail[id].maxNeed,
                g_audioFail[id].minLargest == 0xFFFFFFFF ? 0 : g_audioFail[id].minLargest);
    }
    {
        std::lock_guard<std::mutex> lk(g_heap.rangeMutex);
        uint64_t freeBytes = 0;
        uint32_t largest = 0;
        for (const auto& [a, s] : g_heap.physFree)
        {
            freeBytes += s;
            if (s > largest)
                largest = s;
        }
        uint64_t used = 0;
        for (const auto& [a, s] : g_heap.physUsed)
            used += s;
        fprintf(stderr, "[heapcensus] physical arena (512 MB, outside the heaps): %llu bytes "
                        "free in %zu ranges, largest %u; %llu used in %zu blocks\n",
                (unsigned long long)freeBytes, g_heap.physFree.size(), largest,
                (unsigned long long)used, g_heap.physUsed.size());
    }
    g_lastCensus = t;
    g_streamerMinLargest = 0xFFFFFFFF;
    memset(g_levelFrames, 0, sizeof g_levelFrames);
}

void MaybeEventCensus(PPCContext& ctx, uint8_t* base, const char* why)
{
    const double t = Now();
    if (t - g_lastEventCensus < 5.0)
        return;
    g_lastEventCensus = t;
    Census(ctx, base, why);
}
}   // namespace

// Called by zombie_heap.cpp's sub_8277DD40 hook after every heap creation.
void HeapCensus_OnCreate(uint8_t* base, uint32_t lr, uint32_t heapBase, uint32_t size,
                         uint32_t name, uint32_t parent, int32_t id)
{
    if (!Enabled())
        return;
    const char* s = (name >= 0x82000000 && name < 0x83000000)
                        ? reinterpret_cast<const char*>(base + name) : "?";
    fprintf(stderr, "[heapcensus] create heap %d '%.48s' size %u at %08X arg4 %u caller %08X\n",
            id, s, size, heapBase, parent, lr);
}

// The streamer's memory check.
PPC_FUNC(sub_8222BD70)
{
    const uint32_t streamer = ctx.r3.u32;
    __imp__sub_8222BD70(ctx, base);
    if (!Enabled())
        return;
    std::lock_guard<std::mutex> lk(g_mutex);
    const uint32_t g = PPC_LOAD_U32(kStreamerGlobal);
    g_streamerHeap = g ? int(PPC_LOAD_U32(g + kStreamerHeapOff)) : -1;
    const uint32_t level = PPC_LOAD_U32(streamer + 8);
    g_streamerTotal = PPC_LOAD_U32(streamer + 0xC);
    g_streamerLargest = PPC_LOAD_U32(streamer + 0x10);
    if (g_streamerLargest < g_streamerMinLargest)
        g_streamerMinLargest = g_streamerLargest;
    if (level < 4)
        ++g_levelFrames[level];
    if (level != g_streamerLevel)
    {
        fprintf(stderr, "[heapcensus] t=%.1fs streamer level %u -> %u (heap %d largest free %u, "
                        "total free %u)\n",
                Now(), g_streamerLevel, level, g_streamerHeap, g_streamerLargest, g_streamerTotal);
        const uint32_t was = g_streamerLevel;
        g_streamerLevel = level;
        if (level >= 2 && was < level)
            MaybeEventCensus(ctx, base, "streamer level rose");
    }
    if (Now() - g_lastCensus >= Interval())
        Census(ctx, base, "periodic");
}

// Largest free block: watched only for the audio sample allocation's check.
PPC_FUNC(sub_8277C1A8)
{
    const bool audio = ctx.lr == kAudioSampleCheckReturn;
    const uint32_t id = ctx.r3.u32;
    __imp__sub_8277C1A8(ctx, base);
    if (!audio || !Enabled() || id >= kMaxHeaps)
        return;
    const uint32_t largest = ctx.r3.u32;
    const uint32_t need = ctx.r31.u32;   // the caller's register, restored by the callee
    std::lock_guard<std::mutex> lk(g_mutex);
    ++g_audioChecks[id];
    if (need > g_audioMaxNeed[id])
        g_audioMaxNeed[id] = need;
    if (largest < need)
    {
        AudioFail& f = g_audioFail[id];
        if (f.count < 20 || (f.count % 100) == 0)
            fprintf(stderr, "[heapcensus] t=%.1fs audio sample alloc FAILS on heap %d: need %u, "
                            "largest free %u (failure #%u on this heap)\n",
                    Now(), id, need, largest, f.count + 1);
        ++f.count;
        if (need > f.maxNeed)
            f.maxNeed = need;
        if (largest < f.minLargest)
            f.minLargest = largest;
        MaybeEventCensus(ctx, base, "audio sample allocation failed");
    }
}
