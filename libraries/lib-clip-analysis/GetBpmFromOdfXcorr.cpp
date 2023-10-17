#include "GetBpmFromOdfXcorr.h"

#include "CircularSampleBuffer.h"
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
   else
      // Now we are at bar level or below: only check binary or ternary
      // divisions (although we could also check 5 for Take Five ?).
      return { 2, 3 };
}

struct XCorr
{
   const std::vector<float>& values;
   const double mean;
   const int numPeaks;
};

std::vector<int> DivideEqually(int M, int numDivs)
{
   std::vector<int> i(numDivs - 1);
   std::iota(i.begin(), i.end(), 1);
   std::vector<int> indices(i.size());
   std::transform(i.begin(), i.end(), indices.begin(), [&](double i) {
      return static_cast<int>(std::round(M * i / numDivs));
   });
   // Discard indices beyond the symmetry midpoint.
   indices.erase(
      std::find_if(
         indices.begin(), indices.end(), [M](int i) { return i > M / 2; }),
      indices.end());
   return indices;
}

float GetStandardDeviation(const std::vector<float>& x)
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

std::vector<int>
GetEvalIndices(int M, int numDivs, const std::vector<int>& prevIndices)
{
   const auto newIndices = DivideEqually(M, numDivs);
   std::vector<int> evalIndices;
   std::set_difference(
      newIndices.begin(), newIndices.end(), prevIndices.begin(),
      prevIndices.end(), std::inserter(evalIndices, evalIndices.begin()));
   return evalIndices;
}

std::vector<float>
GetValues(const std::vector<float>& xcorr, const std::vector<int>& indices)
{
   std::vector<float> values(indices.size());
   std::transform(indices.begin(), indices.end(), values.begin(), [&](int i) {
      return xcorr[i];
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

std::optional<TimeSig> MatchesSomeTimeSig(const std::vector<int>& divs)
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

std::string ToString(int numBars, TimeSig sig)
{
   std::stringstream ss;
   ss << numBars << " * " << ToString(sig);
   return ss.str();
}

void recursion(
   const XCorr& xcorr, ResultMap& results, std::map<int, TimeDiv*>& subTimeDivs,
   double cumScore = 1, std::vector<int> divs = {})
{
   const auto M = xcorr.values.size();
   const auto cumDiv =
      std::accumulate(divs.begin(), divs.end(), 1, std::multiplies<int>());
   const auto prevIndices = DivideEqually(M, cumDiv);
   const auto recursionLevel = divs.size();
   const auto divisorCandidates = GetDivisorCandidates(recursionLevel);
   for (const auto divisor : divisorCandidates)
   {
      const auto newDiv = cumDiv * divisor;
      if (newDiv > xcorr.numPeaks)
         continue;

      auto nextDivs = divs;
      nextDivs.push_back(divisor);

      const auto evalIndices = GetEvalIndices(M, newDiv, prevIndices);
      const auto values = GetValues(xcorr.values, evalIndices);
      const auto maxScore =
         values.empty() ? 1. : *std::max_element(values.begin(), values.end());
      if (maxScore < xcorr.mean)
         continue;
      const auto varScore = 1. - GetStandardDeviation(values);
      const auto score = varScore * maxScore;
      subTimeDivs[divisor] =
         new TimeDiv(score, score * cumScore, maxScore, varScore);
      if (const auto timeSig = MatchesSomeTimeSig(nextDivs))
      {
         const auto numBars = nextDivs[0];
         // Do not penalize time signatures that have more divisions.
         const auto finalScore = std::pow(
            subTimeDivs.at(divisor)->cumScore,
            GetBalancingScorePower(nextDivs));
         // TODO account for bpm
         results.emplace(ToString(numBars, *timeSig), finalScore);
         // We have the score for a time signature, we don't need to test its
         // subdivisions.
         continue;
      }
      recursion(
         xcorr, results, subTimeDivs.at(divisor)->subs, score * cumScore,
         nextDivs);
   }
}

int GetNumPeaks(const std::vector<float>& xcorr)
{
   int numPeaks = 1;
   for (auto i = 1; i < xcorr.size() - 1; ++i)
   {
      if (xcorr[i] > xcorr[i - 1] && xcorr[i] > xcorr[i + 1])
         ++numPeaks;
   }
   return numPeaks;
}
} // namespace

std::optional<double>
GetBpmFromOdfXcorr(const std::vector<float>& xcorr, double playDur)
{
   assert(xcorr[0] == 1.); // Expects normalized
   const auto xcorrMean =
      std::accumulate(xcorr.begin(), xcorr.end(), 0.) / xcorr.size();
   const auto numPeaks = GetNumPeaks(xcorr);
   XCorr xcorrStruct { xcorr, xcorrMean, numPeaks };
   std::map<int, TimeDiv*> divs;
   ResultMap results;
   recursion(xcorrStruct, results, divs);

   return 0;
}
} // namespace ClipAnalysis
