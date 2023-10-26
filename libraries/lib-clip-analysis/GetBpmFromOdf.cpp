#include "GetBpmFromOdf.h"

#include "ClipAnalysisUtils.h"
#include "ClipInterface.h"
#include "FFT.h"
#include "OnsetDetector.h"
#include "TimeDiv.h"

#include <array>
#include <fstream>
#include <map>
#include <numeric>
#include <sstream>

namespace ClipAnalysis
{
namespace
{
std::vector<int> GetDivisorCandidates(size_t recursionLevel)
{
   if (recursionLevel == 0)
   {
      // Try everything from 1 to 8 bars.
      std::vector<int> divs(8);
      std::iota(divs.begin(), divs.end(), 1);
      return divs;
   }
   else if (recursionLevel == 1)
      // 2/2 or 6/8, 3/4 or 9/8, or 4/4
      return { 2, 3, 4 };
   else
      // Any of the above might be
      return { 2, 3 };
}

std::vector<int> DivideEqually(int M, int numDivs)
{
   std::vector<int> i(numDivs);
   std::iota(i.begin(), i.end(), 0);
   std::vector<int> indices(i.size());
   std::transform(i.begin(), i.end(), indices.begin(), [&](double i) {
      return static_cast<int>(std::round(M * i / numDivs));
   });
   return indices;
}

float GetStandardDeviation(const std::vector<double>& x)
{
   if (x.empty())
      // Normally NaN, but ...
      return 0;
   const auto mean = std::accumulate(x.begin(), x.end(), 0.) / x.size();
   float accum = 0.f;
   std::for_each(x.begin(), x.end(), [&](const float d) {
      accum += (d - mean) * (d - mean);
   });
   return std::sqrt(accum / x.size());
}

std::vector<int> GetEvalIndices(
   int numDivs, const std::vector<int>& prevIndices,
   const std::vector<double>& odf)
{
   const auto newIndices = DivideEqually(odf.size(), numDivs);
   std::vector<int> evalIndices;
   std::set_difference(
      newIndices.begin(), newIndices.end(), prevIndices.begin(),
      prevIndices.end(), std::inserter(evalIndices, evalIndices.begin()));
   // Find nearest peak
   std::transform(
      evalIndices.begin(), evalIndices.end(), evalIndices.begin(), [&](int i) {
         while (true)
         {
            const auto h = (i - 1) % odf.size();
            const auto j = (i + 1) % odf.size();
            if (odf[h] <= odf[i] && odf[i] >= odf[j])
               return i;
            i = odf[h] > odf[j] ? h : j;
         };
      });
   return evalIndices;
}

std::vector<double>
GetValues(const std::vector<double>& odf, const std::vector<int>& indices)
{
   std::vector<double> values(indices.size());
   std::transform(indices.begin(), indices.end(), values.begin(), [&](int i) {
      return odf[i];
   });
   return values;
}

enum class TimeSig
{
   FourFour,
   ThreeFour,
   SixEight,
   _Count
};

static const std::array<std::vector<int>, static_cast<int>(TimeSig::_Count)>
   timeSigDivs { std::vector<int> { 2, 2 }, std::vector<int> { 3 },
                 std::vector<int> { 2, 3 } };

bool IsTimeSigLevel(TimeSig sig, const std::vector<int>& divs)
{
   if (divs.empty())
      return false;
   const auto& sigDivs = timeSigDivs[static_cast<int>(sig)];
   // We ignore the first entry in divs because it corresponds to the number of
   // bars in the loop.
   return sigDivs == std::vector<int> { divs.begin() + 1, divs.end() };
}

std::optional<TimeSig> GetMatchingTimeSig(const std::vector<int>& divs)
{
   constexpr std::array<TimeSig, static_cast<int>(TimeSig::_Count)> timeSigs {
      TimeSig::FourFour, TimeSig::ThreeFour, TimeSig::SixEight
   };
   const auto it =
      std::find_if(timeSigs.begin(), timeSigs.end(), [&](TimeSig sig) {
         return IsTimeSigLevel(sig, divs);
      });
   return it == timeSigs.end() ? std::nullopt : std::make_optional(*it);
}

double GetBalancingScorePower(const std::vector<int>& divs)
{
   assert(divs.size() > 1);
   // If the 1st division was 1, reduce by one because it'd otherwise be unfair.
   if (divs[0] == 1)
      return 1. / (divs.size() - 1);
   else
      return 1. / divs.size();
}

using ResultMap = std::unordered_map<std::string, double>;

std::string ToString(TimeSig sig)
{
   switch (sig)
   {
   case TimeSig::FourFour:
      return "4/4";
   case TimeSig::ThreeFour:
      return "3/4";
   case TimeSig::SixEight:
      return "6/8";
   default:
      assert(false);
   }
}

double GetBpmProbability(TimeSig sig, int numBars, double loopDuration)
{
   static const std::unordered_map<TimeSig, double> expectedValues {
      { TimeSig::FourFour, 115. },
      { TimeSig::ThreeFour, 140. },
      { TimeSig::SixEight, 64. }
   };
   // TODO 3/4 and 6/8 deviations may be mixed up - review.
   static const std::unordered_map<TimeSig, double> expectedDeviations {
      { TimeSig::FourFour, 25. },
      { TimeSig::ThreeFour, 25. },
      { TimeSig::SixEight, 15. }
   };
   static const std::unordered_map<TimeSig, int> beatsPerBar {
      { TimeSig::FourFour, 4 },
      { TimeSig::ThreeFour, 3 },
      { TimeSig::SixEight, 2 }
   };
   const auto mu = expectedValues.at(sig);
   const auto sigma = expectedDeviations.at(sig);
   const auto barDur = loopDuration / numBars;
   const auto beatDur = barDur / beatsPerBar.at(sig);
   const auto bpm = 60 / beatDur;
   // Do not scale by 1. / (sigma * std::sqrt(2 * M_PI)) such that the peak
   // reaches 1.
   const auto tmp = (bpm - mu) / sigma;
   return std::exp(-.5 * tmp * tmp);
}

std::string ToString(int numBars, TimeSig sig)
{
   std::stringstream ss;
   ss << numBars << " * " << ToString(sig);
   return ss.str();
}

std::vector<std::pair<size_t, size_t>> GetBeatIndexPairs(int numBeats, int div)
{
   assert(numBeats % div == 0);
   std::vector<std::pair<size_t, size_t>> pairs;
   const auto step = numBeats / div;
   for (auto i = 0; i < numBeats; ++i)
      for (auto j = i + step; j < numBeats; j += step)
         pairs.emplace_back(i, j);
   return pairs;
}

std::vector<double> GetBeatPairDistances(
   const std::vector<std::pair<size_t, size_t>>& beatIndexPairs, const ODF& odf)
{
   std::vector<double> distances(beatIndexPairs.size());
   std::transform(
      beatIndexPairs.begin(), beatIndexPairs.end(), distances.begin(),
      [&](const auto& indices) {
         const auto diff = odf.values[odf.beatIndices[indices.first]] -
                           odf.values[odf.beatIndices[indices.second]];
         return diff * diff;
      });
   return distances;
}

void recursion(
   const ODF& odf, ResultMap& results, std::map<int, TimeDiv2*>& subTimeDivs,
   std::vector<int> divs = {})
{
   const auto M = odf.values.size();
   const auto cumDiv =
      std::accumulate(divs.begin(), divs.end(), 1, std::multiplies<int>());
   const auto recursionLevel = divs.size();
   const auto numBeats = odf.beatIndices.size();

   std::vector<double> beatValues(numBeats);
   std::transform(
      odf.beatIndices.begin(), odf.beatIndices.end(), beatValues.begin(),
      [&](size_t i) { return odf.values[i]; });

   for (const auto divisor : GetDivisorCandidates(recursionLevel))
   {
      const auto newDiv = cumDiv * divisor;
      if (numBeats % newDiv != 0)
         continue;

      const auto dissimilarity = GetDissimilarityValue(beatValues, newDiv);
      subTimeDivs[divisor] = new TimeDiv2(dissimilarity);
      auto nextDivs = divs;
      nextDivs.push_back(divisor);
      if (const auto timeSig = GetMatchingTimeSig(nextDivs))
      {
         const auto numBars = nextDivs[0];
         // Do not penalize time signatures that have more divisions.
         const auto bpmProb =
            GetBpmProbability(*timeSig, numBars, odf.duration);
         results.emplace(ToString(numBars, *timeSig), bpmProb);
         // We have the score for a time signature, we don't need to test its
         // subdivisions.
         continue;
      }

      if (recursionLevel < 3)
         recursion(odf, results, subTimeDivs.at(divisor)->subs, nextDivs);
   }
}
} // namespace

double
GetDissimilarityValue(const std::vector<double>& beatValues, int numPeriods)
{
   if (numPeriods == 1)
      return 0.;
   const auto numBeats = beatValues.size();
   const auto pairs = GetBeatIndexPairs2(numBeats, numPeriods);
   return std::accumulate(
             pairs.begin(), pairs.end(), 0.,
             [&](double sum, const auto& indices) {
                const auto diff =
                   beatValues[indices.first] - beatValues[indices.second];
                return sum + diff * diff;
             }) /
          pairs.size();
}

std::vector<std::pair<size_t, size_t>>
GetBeatIndexPairs2(int numBeats, int numPeriods)
{
   assert(numBeats % numPeriods == 0);
   const auto beatsPerPeriod = numBeats / numPeriods;
   auto distanceSum = 0.;
   auto distanceCount = 0;
   std::vector<std::pair<size_t, size_t>> pairs;
   // Compare periods with each other ...
   for (auto p1 = 0; p1 < numPeriods - 1; ++p1)
      for (auto p2 = p1 + 1; p2 < numPeriods; ++p2)
         // ... but don't mix-up beats.
         for (auto b = 1; b < beatsPerPeriod; ++b)
            pairs.emplace_back(
               p1 * beatsPerPeriod + b, p2 * beatsPerPeriod + b);
   return pairs;
}

std::optional<double> GetBpmFromOdf(const ODF& odf)
{
   std::map<int, TimeDiv2*> divs;
   ResultMap results;
   recursion(odf, results, divs);
   return 0;
}
} // namespace ClipAnalysis
