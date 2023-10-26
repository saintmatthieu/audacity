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
   const auto pairs = GetBeatIndexPairs(numBeats, numPeriods);
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

std::vector<std::pair<size_t, size_t>>
GetBeatIndexPairs(int numBeats, int numPeriods)
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
