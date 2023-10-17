#include "GetBpmFromOdfXcorr.h"

#include "CircularSampleBuffer.h"
#include "ClipInterface.h"
#include "FFT.h"
#include "OnsetDetector.h"

#include <array>
#include <fstream>
#include <map>
#include <numeric>
#include <sstream>

namespace
{
constexpr auto numPrimes = 4;
constexpr std::array<int, numPrimes> primes { 2, 3, 5, 7 };

struct TimeDiv
{
   TimeDiv(
      double score, double cumScore = 1, double maxScore = 1,
      double varScore = 1)
       : score(score)
       , cumScore(cumScore)
       , maxScore(maxScore)
       , varScore(varScore)
   {
      subs.fill(nullptr);
   }

   ~TimeDiv()
   {
      for (auto p = 0; p < numPrimes; ++p)
         if (subs[p])
            delete subs[p];
   }

   const double score;
   const double cumScore;
   const double maxScore;
   const double varScore;
   std::array<TimeDiv*, numPrimes> subs;
};

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

bool MatchesSomeTimeSig(const std::vector<int>& divs)
{
   constexpr std::array<TimeSig, static_cast<int>(TimeSig::_Count)> timeSigs {
      TimeSig::FourFour, TimeSig::ThreeFour, TimeSig::SixEight
   };
   return std::any_of(timeSigs.begin(), timeSigs.end(), [&](TimeSig sig) {
      return IsTimeSigLevel(sig, divs);
   });
}

void recursion(
   const XCorr& xcorr, std::array<TimeDiv*, numPrimes>& subTimeDivs,
   double cumScore = 1, std::vector<int> divs = {})
{
   const auto M = xcorr.values.size();
   const auto cumDiv =
      std::accumulate(divs.begin(), divs.end(), 1, std::multiplies<int>());
   const auto prevIndices = DivideEqually(M, cumDiv);
   for (auto p = 0; p < numPrimes; ++p)
   {
      const auto newDiv = cumDiv * primes[p];
      if (newDiv > xcorr.numPeaks)
         continue;

      auto nextDivs = divs;
      nextDivs.push_back(primes[p]);

      const auto evalIndices = GetEvalIndices(M, newDiv, prevIndices);
      const auto values = GetValues(xcorr.values, evalIndices);
      const auto maxScore = *std::max_element(values.begin(), values.end());
      if (maxScore < xcorr.mean)
         continue;
      const auto varScore = 1. - GetStandardDeviation(values);
      const auto score = varScore * maxScore;
      subTimeDivs[p] = new TimeDiv(score, score * cumScore, maxScore, varScore);
      if (MatchesSomeTimeSig(nextDivs))
         // We have the score for a time signature, we don't need to test its
         // subdivisions.
         continue;
      recursion(xcorr, subTimeDivs[p]->subs, score * cumScore, nextDivs);
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

namespace ClipAnalysis
{
std::optional<double>
GetBpmFromOdfXcorr(const std::vector<float>& xcorr, double playDur)
{
   assert(xcorr[0] == 1.); // Expects normalized
   const auto xcorrMean =
      std::accumulate(xcorr.begin(), xcorr.end(), 0.) / xcorr.size();
   const auto numPeaks = GetNumPeaks(xcorr);
   XCorr xcorrStruct { xcorr, xcorrMean, numPeaks };
   std::array<TimeDiv*, numPrimes> divs;
   recursion(xcorrStruct, divs);

   return 0;
}
} // namespace ClipAnalysis
