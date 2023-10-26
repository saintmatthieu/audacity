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

enum class TimeSignature
{
   FourFour,
   ThreeFour,
   SixEight,
   _Count,
};

struct Result
{
   const int numBars;
   const double quarterNotesPerMinute;
   const TimeSignature timeSignature;
};

CLIP_ANALYSIS_API std::optional<Result> GetBpmFromOdf(const ODF& odf);

CLIP_ANALYSIS_API std::vector<std::pair<size_t, size_t>>
GetBeatIndexPairs(int numBeats, int numPeriods);
} // namespace ClipAnalysis
