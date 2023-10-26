#pragma once

#include <optional>
#include <vector>

namespace ClipAnalysis
{
struct ODF
{
   std::vector<double> values;
   const double duration;
   const std::vector<size_t> beatIndices; // TODO this will have to be guessed.
};

CLIP_ANALYSIS_API std::optional<double> GetBpmFromOdf(const ODF& odf);

CLIP_ANALYSIS_API std::vector<std::pair<size_t, size_t>>
GetBeatIndexPairs2(int numBeats, int numPeriods);

CLIP_ANALYSIS_API double
GetDissimilarityValue(const std::vector<double>& beatValues, int numPeriods);
} // namespace ClipAnalysis
