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

static const std::array<
   std::vector<int>, static_cast<int>(TimeSignature::_Count)>
   timeSigDivs { std::vector<int> { 2, 2 }, std::vector<int> { 3 },
                 std::vector<int> { 2, 3 } };

bool IsTimeSigLevel(TimeSignature sig, const std::vector<int>& divs)
{
   if (divs.empty())
      return false;
   const auto& sigDivs = timeSigDivs[static_cast<int>(sig)];
   // We ignore the first entry in divs because it corresponds to the number of
   // bars in the loop.
   return sigDivs == std::vector<int> { divs.begin() + 1, divs.end() };
}

std::optional<TimeSignature> GetMatchingTimeSig(const std::vector<int>& divs)
{
   constexpr std::array<TimeSignature, static_cast<int>(TimeSignature::_Count)>
      timeSigs { TimeSignature::FourFour, TimeSignature::ThreeFour,
                 TimeSignature::SixEight };
   const auto it =
      std::find_if(timeSigs.begin(), timeSigs.end(), [&](TimeSignature sig) {
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

std::string ToString(TimeSignature sig)
{
   switch (sig)
   {
   case TimeSignature::FourFour:
      return "4/4";
   case TimeSignature::ThreeFour:
      return "3/4";
   case TimeSignature::SixEight:
      return "6/8";
   default:
      assert(false);
   }
}

const std::unordered_map<TimeSignature, int> beatsPerBar {
   { TimeSignature::FourFour, 4 },
   { TimeSignature::ThreeFour, 3 },
   { TimeSignature::SixEight, 2 }
};

double GetBpmProbability(TimeSignature sig, int numBars, double loopDuration)
{
   static const std::unordered_map<TimeSignature, double> expectedValues {
      { TimeSignature::FourFour, 115. },
      { TimeSignature::ThreeFour, 140. },
      { TimeSignature::SixEight, 64. }
   };
   // TODO 3/4 and 6/8 deviations may be mixed up - review.
   static const std::unordered_map<TimeSignature, double> expectedDeviations {
      { TimeSignature::FourFour, 25. },
      { TimeSignature::ThreeFour, 25. },
      { TimeSignature::SixEight, 15. }
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

std::string ToString(int numBars, TimeSignature sig)
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

struct Leaf
{
   double dissimilaritySum = 0;
   int numComparisons = 0;
   double bpmProbability;
};

using Branch = std::unordered_map<TimeSignature, Leaf>;
using Tree = std::map<int /*num bars*/, Branch>;

void EvaluateDissimilarities(
   const std::vector<double>& beatValues, int numPeriods, double& sum,
   int& numComparisons)
{
   if (numPeriods == 1)
   {
      sum = 0.;
      numComparisons = 0;
      return;
   }
   const auto numBeats = beatValues.size();
   const auto pairs = GetBeatIndexPairs2(numBeats, numPeriods);
   sum = std::accumulate(
      pairs.begin(), pairs.end(), 0., [&](double sum, const auto& indices) {
         const auto diff =
            beatValues[indices.first] - beatValues[indices.second];
         return sum + diff * diff;
      });
   numComparisons = pairs.size();
}

void FillTree(const ODF& odf, Tree& tree)
{
   const auto numBeats = odf.beatIndices.size();
   std::vector<double> beatValues(numBeats);
   std::transform(
      odf.beatIndices.begin(), odf.beatIndices.end(), beatValues.begin(),
      [&](size_t i) { return odf.values[i]; });

   for (const auto numBars : { 1, 2, 3, 4, 5, 6, 7, 8 })
   {
      if (numBeats % numBars != 0)
         continue;

      auto branchSum = 0.;
      auto branchNumComparisons = 0;
      EvaluateDissimilarities(
         beatValues, numBars, branchSum, branchNumComparisons);
      Branch branch;
      for (auto timeSignature :
           { TimeSignature::FourFour, TimeSignature::ThreeFour,
             TimeSignature::SixEight })
      {
         const auto numPeriods = numBars * beatsPerBar.at(timeSignature);
         if (numBeats % numPeriods != 0)
            continue;
         Leaf& leaf = branch[timeSignature];
         leaf.bpmProbability =
            GetBpmProbability(timeSignature, numBars, odf.duration);
         EvaluateDissimilarities(
            beatValues, numPeriods, leaf.dissimilaritySum, leaf.numComparisons);
         leaf.dissimilaritySum += branchSum;
         leaf.numComparisons += branchNumComparisons;
      }
      if (!branch.empty())
         tree[numBars] = std::move(branch);
   }
}

} // namespace

std::vector<std::pair<size_t, size_t>>
GetBeatIndexPairs2(int numBeats, int numPeriods)
{
   assert(numBeats % numPeriods == 0);
   const auto beatsPerPeriod = numBeats / numPeriods;
   std::vector<int> beatRecursionLevels(numBeats);
   std::vector<int> beatPeriodIndices(numBeats);
   for (auto b = 0; b < numBeats; ++b)
   {
      beatRecursionLevels[b] = b == 0 ? 0 : b % beatsPerPeriod == 0 ? 1 : 2;
      beatPeriodIndices[b] = b / beatsPerPeriod;
   }
   std::vector<std::pair<size_t, size_t>> pairs;
   // Fill `pairs` with beat indices of equal recursion level and different
   // period index.
   for (auto b1 = 0; b1 < numBeats; ++b1)
      for (auto b2 = b1 + 1; b2 < numBeats; ++b2)
         if (
            beatRecursionLevels[b1] == beatRecursionLevels[b2] &&
            beatPeriodIndices[b1] != beatPeriodIndices[b2])
            pairs.emplace_back(b1, b2);
   return pairs;
}

namespace
{
auto GetWinnerLeaf(const Branch& branch)
{
   return std::min_element(
      branch.begin(), branch.end(),
      [](const std::pair<TimeSignature, Leaf>& a,
         const std::pair<TimeSignature, Leaf>& b) {
         return a.second.dissimilaritySum / a.second.numComparisons <
                b.second.dissimilaritySum / b.second.numComparisons;
      });
}
} // namespace

std::optional<Result> GetBpmFromOdf(const ODF& odf)
{
   Tree tree;
   FillTree(odf, tree);
   if (tree.empty())
      return {};
   const auto& [numBars, branch] = *std::min_element(
      tree.begin(), tree.end(),
      [](const std::pair<int, Branch>& a, const std::pair<int, Branch>& b) {
         const auto& winnerLeafA = GetWinnerLeaf(a.second)->second;
         const auto& winnerLeafB = GetWinnerLeaf(b.second)->second;
         // TODO integrate bpm probability
         return winnerLeafA.dissimilaritySum / winnerLeafA.numComparisons <
                winnerLeafB.dissimilaritySum / winnerLeafB.numComparisons;
      });
   const auto& [timeSignature, leaf] = *GetWinnerLeaf(branch);
   const auto barsPerMinute = numBars * 60 / odf.duration;
   const auto quarternotesPerMinute =
      // 6/8 two beats but three quarter notes per bar.
      timeSignature == TimeSignature::SixEight ?
         3 * barsPerMinute :
         beatsPerBar.at(timeSignature) * barsPerMinute;
   return Result { numBars, quarternotesPerMinute, timeSignature };
}
} // namespace ClipAnalysis
