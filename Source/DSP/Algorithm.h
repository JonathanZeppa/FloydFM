#pragma once

#include <array>
#include <cstdint>

// =====================================================================
//  Algorithm.h -- SINGLE SOURCE OF TRUTH for the FM routing.
//
//  FLOYD-FM-HANDOFF.md > "algorithm -- canonical Yamaha 4-op set":
//  "This table lives in exactly one header (Algorithm.h). The routing
//  diagram component and the DSP voice both read from it. Never
//  maintain a parallel table for the visual."
//
//  Same connections as DX21 / DX27 / DX100 / TX81Z / YM2151 (OPM
//  connections 0-7). Operators are 0-indexed internally, displayed as
//  OP1-OP4. Edges are [src, dst] meaning src modulates dst.
// =====================================================================

namespace floyd
{

inline constexpr int kNumOps        = 4;
inline constexpr int kNumAlgorithms = 8;
inline constexpr int kMaxEdges      = 3;

struct Edge
{
    std::uint8_t src;
    std::uint8_t dst;
};

struct Algorithm
{
    int         display;      // 1..8, shown as "ALG n"
    const char* name;         // "STACK"
    const char* label;        // "1>2>3>4" -- the ALG label string
    std::uint8_t edgeCount;
    std::array<Edge, kMaxEdges> edges;
    std::uint8_t carriers;    // bitmask, bit N set => op N is a carrier
};

inline constexpr std::uint8_t carrierMask (std::uint8_t a) { return (std::uint8_t) (1u << a); }

// Handoff table, row for row. `label` strings come from the canonical
// mockup's ALGORITHMS array. ASCII only.
inline constexpr std::array<Algorithm, kNumAlgorithms> kAlgorithms { {
    { 1, "STACK",     "1>2>3>4",     3, { { {0,1}, {1,2}, {2,3} } }, 0b1000 },
    { 2, "TWIN FEED", "(1+2)>3>4",   3, { { {0,2}, {1,2}, {2,3} } }, 0b1000 },
    { 3, "SPLIT MOD", "1+(2>3)>4",   3, { { {1,2}, {0,3}, {2,3} } }, 0b1000 },
    { 4, "BRANCH",    "(1>2)+3>4",   3, { { {0,1}, {1,3}, {2,3} } }, 0b1000 },
    { 5, "TWIN PAIR", "(1>2)+(3>4)", 2, { { {0,1}, {2,3} } },        0b1010 },
    { 6, "FAN",       "1>2,3,4",     3, { { {0,1}, {0,2}, {0,3} } }, 0b1110 },
    { 7, "PAIR+SINE", "1>2 +3+4",    1, { { {0,1} } },               0b1110 },
    { 8, "ADDITIVE",  "1+2+3+4",     0, { {} },                      0b1111 },
} };

inline constexpr const Algorithm& algorithmAt (int index)
{
    return kAlgorithms[(std::size_t) (index < 0 ? 0 : (index >= kNumAlgorithms ? kNumAlgorithms - 1 : index))];
}

inline constexpr bool isCarrier (const Algorithm& a, int op)
{
    return (a.carriers & carrierMask ((std::uint8_t) op)) != 0;
}

// An operator's level is 1 + max(level of everything it modulates);
// level 0 = carriers. Used by the routing diagram for row placement AND
// by the voice to tick modulators before their destinations.
struct Levels
{
    std::array<std::uint8_t, kNumOps> level {};
    std::uint8_t maxLevel = 0;
};

inline Levels computeLevels (const Algorithm& a)
{
    Levels lv;

    // Relax kNumOps times -- the DAG is at most kNumOps deep.
    for (int pass = 0; pass < kNumOps; ++pass)
        for (std::uint8_t e = 0; e < a.edgeCount; ++e)
        {
            const auto& edge = a.edges[e];
            const auto  want = (std::uint8_t) (lv.level[edge.dst] + 1);
            if (want > lv.level[edge.src])
                lv.level[edge.src] = want;
        }

    for (int i = 0; i < kNumOps; ++i)
        if (lv.level[i] > lv.maxLevel)
            lv.maxLevel = lv.level[i];

    return lv;
}

// Render order: highest level (deepest modulator) first, so every
// operator's modulation input is complete before it ticks.
struct RenderOrder
{
    std::array<std::uint8_t, kNumOps> op {};
};

inline RenderOrder computeRenderOrder (const Algorithm& a)
{
    const auto lv = computeLevels (a);

    RenderOrder order;
    int n = 0;

    for (int level = (int) lv.maxLevel; level >= 0; --level)
        for (std::uint8_t i = 0; i < kNumOps; ++i)
            if (lv.level[i] == level)
                order.op[(std::size_t) n++] = i;

    return order;
}

} // namespace floyd
