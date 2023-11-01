#pragma once

#include "ODF.h"

#include <NamedType/named_type.hpp>

#include <optional>
#include <vector>

class ClipInterface;

namespace ClipAnalysis
{
struct ODF;

using Level = fluent::NamedType<unsigned, struct LevelTag, fluent::Arithmetic>;
using L = Level;

using Ordinal =
   fluent::NamedType<size_t, struct OrdinalTag, fluent::Arithmetic>;
using O = Ordinal;

struct AltResult
{
   AltResult(
      int numBars, double quarternotesPerMinute, TimeSignature timeSignature)
       : numBars { numBars }
       , quarternotesPerMinute { quarternotesPerMinute }
       , timeSignature { timeSignature }
   {
   }
   const int numBars;
   const double quarternotesPerMinute;
   const TimeSignature timeSignature;
};

struct Result
{
   int numBars;
   double quarternotesPerMinute;
   TimeSignature timeSignature;
   std::optional<AltResult> alternative;
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
