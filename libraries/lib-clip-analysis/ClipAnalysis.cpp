#include "ClipAnalysis.h"

#include "GetBpmFromOdf.h"
#include "GetOdf.h"

namespace ClipAnalysis
{
// Assuming that odfVals are onsets of a loop. Then there must be a round number
// of beats. Currently assuming a 4/4 time signature, it just tried a range of
// number of measures and picks the most likely one. Which is ?..
//
// Fact number 1: tempi abide by a distribution that might well be approximated
// with a gaussian PDF. Looking up some numbers on the net, this could have an
// average around 115 bpm and a variance of 25 bpm. This is given a weight in
// the comparison of hypotheses.
//
// Second, in most musical genres, and in any case those we expect to see the
// most in a DAW, a piece is a recursive rhythmic construction: the sections of
// a piece may have common time divisors ; the number of bars in a section is
// often a multiple of 2 ; and then a bar, as time signatures suggest, is
// divisible most often in 4 (4/4), sometimes in 3 (3/4, 6/8) and sometimes
// in 2.
//
// Here we begin simple and assume 4/4. Remains just finding the number of bars.
// The rest is currently an improvisation using the auto correlation of the
// onset detection values, which works halfway, but something more sensible and
// discriminant might be at hand ...

std::optional<MeterInfo>
GetBpm(const ClipInterface& clip, const std::optional<double>& tempoHint)
{
   const auto odf = GetOdf(clip);
   auto result = GetBpmFromOdf2(odf, tempoHint);
   if (result.has_value())
   {
      MeterInfo info;
      info.numBars = result->numBars;
      info.timeSignature = result->timeSignature;
      info.quarternotesPerMinute = result->quarternotesPerMinute;
      if (const auto& alt = result->alternative)
         info.alternative.emplace(
            alt->numBars, alt->timeSignature, alt->quarternotesPerMinute);
      return info;
   }
   else
      return {};
}

int GetNumQuarternotesPerBar(TimeSignature sig)
{
   // 6/8 and 3/4 have both three crotchets per bar.
   return sig == TimeSignature::FourFour ? 4 : 3;
}

} // namespace ClipAnalysis
