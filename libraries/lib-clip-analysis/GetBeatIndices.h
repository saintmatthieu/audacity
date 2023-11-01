#pragma once

#include <optional>
#include <vector>

namespace ClipAnalysis
{
struct ODF;

CLIP_ANALYSIS_API std::optional<std::vector<size_t>>
GetBeatIndices(const ODF& odf, const std::vector<float>& xcorr);

CLIP_ANALYSIS_API bool IsLoop(
   const ODF& odf, const std::vector<float>& xcorr,
   const std::vector<size_t>& beatIndices);
} // namespace ClipAnalysis
