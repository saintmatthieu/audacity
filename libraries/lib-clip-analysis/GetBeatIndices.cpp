#include "GetBeatIndices.h"
#include "ClipAnalysisUtils.h"
#include "ODF.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <fstream>
#include <numeric>
#include <sstream>

namespace ClipAnalysis
{
namespace
{

template <typename T>
std::string PrintVector(const T& vector, const std::string& name)
{
   std::stringstream ss;
   ss << name << " = [";
   auto separator = "";
   for (auto val : vector)
   {
      ss << separator << val;
      separator = ", ";
   }
   ss << "];" << std::endl;
   return ss.str();
}
} // namespace

std::optional<std::vector<size_t>>
GetBeatIndices(const ODF& odf, const std::vector<float>& xcorr)
{
   constexpr auto logValues = false;
   const auto lagPerSample = odf.duration / xcorr.size();
   constexpr auto minBpm = 60;
   constexpr auto maxBpm = 180;
   const auto minSamps = 60. / maxBpm / lagPerSample;
   const auto maxSamps = 60. / minBpm / lagPerSample;

   if (logValues)
   {
      std::ofstream ofs { "C:/Users/saint/Downloads/test.m" };
      ofs << "clear all, close all" << std::endl;
      ofs << PrintVector(odf.values, "odfVals");
      ofs << PrintVector(xcorr, "xcorr");
      ofs << "plot(odfVals), grid" << std::endl;
      ofs
         << "hold on, plot((0:length(interpOdfVals)-1)*length(odfVals)/length(interpOdfVals), interpOdfVals, 'r--'), hold off, grid"
         << std::endl;
      ofs << "lagPerSample = " << lagPerSample << ";\n";
      ofs << "minLag = " << minSamps * lagPerSample << ";\n";
      ofs << "maxLag = " << maxSamps * lagPerSample << ";\n";
      ofs << "figure, plot((0:length(xcorr)-1)*lagPerSample, xcorr)\n";
      ofs << "hold on, plot([minLag, minLag], [0, max(xcorr)])\n";
      ofs << "plot([maxLag, maxLag], [0, max(xcorr)]), hold off\n";
   }

   // Get first peak after minLag whose value is larger than the xcorr average:
   const auto avg = std::accumulate(xcorr.begin(), xcorr.end(), 0.) /
                    static_cast<double>(xcorr.size());
   const auto end = xcorr.begin() + static_cast<int>(maxSamps);

   // Guess beat period
   // TODO do not begin from the last possible beat index up, but from the most
   // likely beat index (i.e., around 120 bpm) and search for the first peak in
   // both up and down directions.
   auto P = std::max<size_t>(minSamps, 1);
   for (; P < maxSamps && P < xcorr.size(); ++P)
      if (xcorr[P] > avg && xcorr[P - 1] < xcorr[P] && xcorr[P] > xcorr[P + 1])
         break;
   if (P == maxSamps || P == xcorr.size())
      return {};

   std::vector<size_t> beatIndices { 0u };
   auto prevP = 0;
   auto i = 0;
   while (true)
   {
      // peak index
      auto j =
         i == 0 ?
            P :
            static_cast<size_t>(static_cast<double>(prevP) * (i + 1) / i + .5);
      if (j >= xcorr.size())
         break;
      while (true)
      {
         const auto a = (j - 1) % xcorr.size();
         const auto b = (j + 1) % xcorr.size();
         if (xcorr[a] <= xcorr[j] && xcorr[j] >= xcorr[b])
            break;
         j = xcorr[a] > xcorr[j] ? a : b;
      }
      if (j < prevP)
         break;
      else if (j == prevP)
         // Doesn't look good ; just give up.
         return {};
      beatIndices.push_back(j);
      prevP = j;
      ++i;
   }
   return beatIndices;
}

bool IsLoop(
   const ODF& odf, const std::vector<float>& xcorr,
   const std::vector<size_t>& beatIndices)
{
   // Should be normalized.
   assert(xcorr[0] == 1.);
   std::vector<int> divisors { 2, 3, 4, 6, 8, 9, 12 };
   const auto numBeats = beatIndices.size();
   // Filter out divisors that aren't divisors of the number of beats
   divisors.erase(
      std::remove_if(
         divisors.begin(), divisors.end(),
         [&](auto divisor) { return numBeats % divisor != 0; }),
      divisors.end());
   // If there isn't a divisor that satisfies this, we're done.
   if (divisors.empty())
      return false;

   std::vector<double> divisorPeriods(divisors.size());
   std::transform(
      divisors.begin(), divisors.end(), divisorPeriods.begin(),
      [&](auto divisor) {
         return static_cast<double>(xcorr.size()) / divisor;
      });
   const auto xcorrLagStep = odf.duration / xcorr.size();
   // In seconds
   std::vector<double> averageDivisorBeatDistances(divisors.size());
   std::transform(
      divisorPeriods.begin(), divisorPeriods.end(),
      averageDivisorBeatDistances.begin(), [&](double divisorPeriod) {
         std::vector<double> beatIndexDistances(numBeats);
         std::transform(
            beatIndices.begin(), beatIndices.end(), beatIndexDistances.begin(),
            [&](auto beatIndex) {
               // Find multiple of divisor period closest to i
               const auto closestDivisorPeriodMultiple =
                  std::round(beatIndex / divisorPeriod) * divisorPeriod;
               const auto distance =
                  (beatIndex - closestDivisorPeriodMultiple) * xcorrLagStep;
               return distance < 0 ? -distance : distance;
            });
         return std::accumulate(
                   beatIndexDistances.begin(), beatIndexDistances.end(), 0.) /
                numBeats;
      });

   std::vector<float> averageDivisorValues(divisors.size());
   std::transform(
      divisors.begin(), divisors.end(), averageDivisorValues.begin(),
      [&](auto divisor) {
         std::vector<float> divisorValues(divisor);
         const auto step = numBeats / divisor;
         for (auto i = 0; i < divisor; ++i)
            divisorValues[i] = xcorr[beatIndices[i * step]];
         return std::accumulate(
                   divisorValues.begin(), divisorValues.end(), 0.f) /
                divisor;
      });

   const auto xcorrMean = std::accumulate(xcorr.begin(), xcorr.end(), 0.) /
                          static_cast<double>(xcorr.size());
   std::vector<double> divisorScores(divisors.size());
   auto i = 0;
   std::for_each(divisorScores.begin(), divisorScores.end(), [&](auto& score) {
      // Scale the distance by -10 to get the log10 of the probability.
      // This means that a distance of 0.1 second yields a probabilty of 0.1.
      const auto distanceLog10Prob = -10 * averageDivisorBeatDistances[i];

      // y(x) = a*log(b*x), with y(1) = 1 and y(0.1) = 0.5. This reduces to
      // y(x) = aÜ(log(x) + 1/a), with a = -1/(2*log(1/10));
      constexpr auto a = 0.217147240951626;
      const auto divisorValue = averageDivisorValues[i];
      const auto x = std::clamp(divisorValue - xcorrMean, 0., 1.);
      const auto peakinessProb = a * (std::log(x) + 1 / a);
      const auto peakinessLog10Prob = std::log10(peakinessProb);
      score = distanceLog10Prob + peakinessLog10Prob;
      ++i;
   });

   const auto bestScore = *std::max_element(
      divisorScores.begin(), divisorScores.end(),
      [](auto a, auto b) { return a < b; });

   return bestScore > -1;
}
} // namespace ClipAnalysis
