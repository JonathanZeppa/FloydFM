#pragma once

#include <array>
#include <cmath>

// =====================================================================
//  RatioTable.h -- the 19 musical ratios, snapped.
//
//  FLOYD-FM-HANDOFF.md > "op{n}_ratio -- snapped choice, not continuous":
//  "Free ratios are deliberately excluded: non-simple ratios throw
//  inharmonic sidebands that read as aliasing (banked lesson).
//  op{n}_detune covers the in-between territory."
//
//  op{n}_ratio is therefore an AudioParameterChoice over these indices,
//  NOT a float. (Choice params do not need CG1 snapBool -- getValue()
//  auto-snaps; only AudioParameterBool has that bug.)
// =====================================================================

namespace floyd
{

inline constexpr int kNumRatios = 19;

inline constexpr std::array<float, kNumRatios> kRatios {
    0.25f, 0.50f, 0.75f, 1.00f, 1.25f, 1.50f, 2.00f, 2.50f, 3.00f, 3.50f,
    4.00f, 5.00f, 6.00f, 7.00f, 8.00f, 9.00f, 10.00f, 11.00f, 12.00f
};

// Handoff: default 3 (= 1.00).
inline constexpr int kDefaultRatioIndex = 3;

inline constexpr float ratioAt (int index)
{
    return kRatios[(std::size_t) (index < 0 ? 0 : (index >= kNumRatios ? kNumRatios - 1 : index))];
}

// Nearest table index to an arbitrary ratio. Used to transcribe the
// mockup's FACTORY array, three of whose values (14.00, 5.25, 8.75) are
// not in the table -- see DISC-3 in floydfm-logfile.md.
inline int nearestRatioIndex (float ratio)
{
    int   best     = 0;
    float bestDist = std::abs (kRatios[0] - ratio);

    for (int i = 1; i < kNumRatios; ++i)
    {
        const float d = std::abs (kRatios[(std::size_t) i] - ratio);
        if (d < bestDist)
        {
            bestDist = d;
            best     = i;
        }
    }

    return best;
}

} // namespace floyd
