#pragma once

#include <optional>
#include <vector>

namespace ClipAnalysis
{
struct ODF;

CLIP_ANALYSIS_API std::optional<std::vector<size_t>>
GetBeatIndices(const ODF& odf);
} // namespace ClipAnalysis
