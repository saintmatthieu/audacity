#pragma once

#include <vector>

namespace ClipAnalysis
{
CLIP_ANALYSIS_API std::vector<float>
GetNormalizedAutocorr(const std::vector<double>& odfValues);
}
