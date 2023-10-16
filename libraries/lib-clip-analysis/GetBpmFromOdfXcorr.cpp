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
   return indices;
}

double GetStandardDeviation(const std::vector<double>& x)
{
   const auto mean = std::accumulate(x.begin(), x.end(), 0.) / x.size();
   double accum = 0.;
   std::for_each(x.begin(), x.end(), [&](const double d) {
      accum += (d - mean) * (d - mean);
   });
   return std::sqrt(accum / x.size());
}

void recursion(
   const XCorr& xcorr, std::array<TimeDiv*, numPrimes>& subTimeDivs,
   double cumScore = 1, int cumDiv = 1, int level = 1)
{
   const auto M = xcorr.values.size();
   const auto prevIndices = DivideEqually(M, cumDiv);
   for (auto p = 0; p < numPrimes; ++p)
   {
      const auto newDiv = cumDiv * primes[p];
      if (newDiv > xcorr.numPeaks)
         continue;
      const auto newIndices = DivideEqually(M, newDiv);
      std::vector<int> uniqueIndices;
      std::set_difference(
         newIndices.begin(), newIndices.end(), prevIndices.begin(),
         prevIndices.end(),
         std::inserter(uniqueIndices, uniqueIndices.begin()));
      std::vector<double> values(uniqueIndices.size());
      std::transform(
         uniqueIndices.begin(), uniqueIndices.end(), values.begin(),
         [&](int i) { return xcorr.values[i]; });
      const auto maxScore = *std::max_element(values.begin(), values.end());
      if (maxScore < xcorr.mean)
         continue;
      const auto varScore = 1. - GetStandardDeviation(values);
      const auto score = varScore * maxScore;
      subTimeDivs[p] = new TimeDiv(score, score * cumScore, maxScore, varScore);
      recursion(xcorr, subTimeDivs[p]->subs, score * cumScore, newDiv, level + 1);
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
