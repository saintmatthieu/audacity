#pragma once

#include <optional>
#include <vector>

namespace ClipAnalysis
{
CLIP_ANALYSIS_API std::optional<double>
GetBpmFromOdf(const std::vector<double>& odf, double playDur);
} // namespace ClipAnalysis
