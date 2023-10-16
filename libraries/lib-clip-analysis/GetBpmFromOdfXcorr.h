#pragma once

#include <optional>
#include <vector>

namespace ClipAnalysis
{
CLIP_ANALYSIS_API std::optional<double>
GetBpmFromOdfXcorr(const std::vector<float>& xcorr, double playDur);
} // namespace ClipAnalysis
