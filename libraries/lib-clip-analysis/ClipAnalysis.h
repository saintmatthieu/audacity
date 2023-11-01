/**********************************************************************

  Audacity: A Digital Audio Editor

  ClipAnalysis.h

  Matthieu Hodgkinson

*******************************************************************/

#pragma once

#include "ODF.h"

#include <memory>
#include <optional>

class ClipInterface;

namespace ClipAnalysis
{
struct AltMeterInfo
{
   AltMeterInfo() = default;
   AltMeterInfo(
      int numBars, TimeSignature timeSignature, double quarternotesPerMinute)
       : numBars { numBars }
       , timeSignature { timeSignature }
       , quarternotesPerMinute { quarternotesPerMinute }
   {
   }
   int numBars = 0;
   TimeSignature timeSignature = TimeSignature::_Count;
   double quarternotesPerMinute = 0.;
};

struct MeterInfo
{
   MeterInfo() = default;
   MeterInfo(
      int numBars, const TimeSignature& timeSignature,
      double quarternotesPerMinute, std::optional<AltMeterInfo> alternative)
       : numBars(numBars)
       , timeSignature(timeSignature)
       , quarternotesPerMinute(quarternotesPerMinute)
       , alternative(std::move(alternative))
   {
   }
   int numBars = 0;
   TimeSignature timeSignature = TimeSignature::_Count;
   double quarternotesPerMinute = 0.;
   std::optional<AltMeterInfo> alternative = {};
};

CLIP_ANALYSIS_API int GetNumQuarternotesPerBar(TimeSignature timeSignature);

CLIP_ANALYSIS_API std::optional<MeterInfo> GetBpm(const ClipInterface& clip);
} // namespace ClipAnalysis
