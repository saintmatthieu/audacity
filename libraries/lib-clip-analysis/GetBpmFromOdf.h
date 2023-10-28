#pragma once

#include <NamedType/named_type.hpp>

#include <optional>
#include <vector>

namespace ClipAnalysis
{
using Level = fluent::NamedType<unsigned, struct LevelTag, fluent::Arithmetic>;
using L = Level;

using Ordinal =
   fluent::NamedType<size_t, struct OrdinalTag, fluent::Arithmetic>;
using O = Ordinal;

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

CLIP_ANALYSIS_API std::optional<Result> GetBpmFromOdf2(const ODF& odf);

CLIP_ANALYSIS_API std::vector<std::pair<size_t, size_t>>
GetBeatIndexPairs(int numBeats, int numPeriods);

struct DivisionLevelInput
{
   int totalNumDivs = 0;
   int numBars = 0;
   TimeSignature timeSignature = TimeSignature::_Count;
};

CLIP_ANALYSIS_API std::optional<std::vector<Level>>
GetDivisionLevels(const DivisionLevelInput& input);

CLIP_ANALYSIS_API std::vector<Ordinal>
ToSeriesOrdinals(const std::vector<Level>& levels);
} // namespace ClipAnalysis
