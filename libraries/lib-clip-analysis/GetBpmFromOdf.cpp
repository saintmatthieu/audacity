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
   const std::vector<double>& beatValues, int numPeriods, int numSubPeriods,
   double odfMean, double& sum, int& numComparisons)
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
            pairs.begin(), pairs.end(), 0.,
            [&](double sum, const auto& indices) {
               const auto diff =
                  beatValues[indices.first] - beatValues[indices.second];
               return sum + diff * diff;
            }) /
         odfMean;
   numComparisons = pairs.size();
}

void FillTree(const ODF& odf, Tree& tree)
{
   const auto numBeats = odf.beatIndices.size();
   std::vector<double> beatValues(numBeats);
   std::transform(
      odf.beatIndices.begin(), odf.beatIndices.end(), beatValues.begin(),
      [&](size_t i) { return odf.values[i]; });
   const auto odfMean =
      std::accumulate(odf.values.begin(), odf.values.end(), 0.) /
      odf.values.size();

   for (const auto numBars : { 1, 2, 3, 4, 5, 6, 7, 8 })
   {
      if (numBeats % numBars != 0)
         continue;

      auto branchSum = 0.;
      auto branchNumComparisons = 0;
      // At bar level, compare everything bar-wise.
      constexpr auto numSubPeriods = 1;
      EvaluateDissimilarities(
         beatValues, numBars, numSubPeriods, odfMean, branchSum,
         branchNumComparisons);
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
         const auto numSubBeats =
            timeSignature == TimeSignature::SixEight ? 3 : 1;
         EvaluateDissimilarities(
            beatValues, numPeriods, numSubBeats, odfMean, leaf.dissimilaritySum,
            leaf.numComparisons);
         leaf.dissimilaritySum += branchSum;
         leaf.numComparisons += branchNumComparisons;
      }
      if (!branch.empty())
         tree[numBars] = std::move(branch);
   }
}

struct Stuff
{
   const double dissimilarity;
   const double bpmProbability;
};

bool operator<(const Stuff& a, const Stuff& b)
{
   const auto bpmA = std::log(a.bpmProbability);
   const auto bpmB = std::log(b.bpmProbability);
   const auto disA = std::log(a.dissimilarity);
   const auto disB = std::log(b.dissimilarity);

   // TODO tune
   const auto disWeight = 10.;
   return bpmA - disA * disWeight < bpmB - disB * disWeight;
}

auto GetWinnerLeaf(const Branch& branch)
{
   return std::max_element(
      branch.begin(), branch.end(),
      [](const std::pair<TimeSignature, Leaf>& a,
         const std::pair<TimeSignature, Leaf>& b) {
         return Stuff { a.second.dissimilaritySum / a.second.numComparisons,
                        a.second.bpmProbability } <
                Stuff { b.second.dissimilaritySum / b.second.numComparisons,
                        b.second.bpmProbability };
      });
}
} // namespace

std::vector<std::pair<size_t, size_t>>
GetBeatIndexPairs(int numBeats, int numPeriods)
{
   assert(numBeats % numPeriods == 0);
   const auto beatsPerPeriod = numBeats / numPeriods;
   std::vector<int> beatRecursionLevels(numBeats);
   {
      auto b = 0;
      std::transform(
         beatRecursionLevels.begin(), beatRecursionLevels.end(),
         beatRecursionLevels.begin(), [&](int) {
            const auto result = b == 0 ? 0 : b % beatsPerPeriod == 0 ? 1 : 2;
            ++b;
            return result;
         });
   }
   std::vector<std::pair<size_t, size_t>> pairs;
   // Fill `pairs` with beat indices of equal recursion level and different
   // period index.
   for (auto p1 = 0; p1 < numPeriods - 1; ++p1)
      for (auto p2 = p1 + 1; p2 < numPeriods; ++p2)
         for (auto b = 0; b < beatsPerPeriod; ++b)
         {
            const auto i = p1 * beatsPerPeriod + b;
            const auto j = p2 * beatsPerPeriod + b;
            if (beatRecursionLevels[i] == beatRecursionLevels[j])
               pairs.emplace_back(i, j);
         }

   return pairs;
}

std::optional<Result> GetBpmFromOdf(const ODF& odf)
{
   Tree tree;
   FillTree(odf, tree);
   if (tree.empty())
      return {};
   const auto& [numBars, branch] = *std::max_element(
      tree.begin(), tree.end(),
      [](const std::pair<int, Branch>& a, const std::pair<int, Branch>& b) {
         const auto& [sigA, leafA] = *GetWinnerLeaf(a.second);
         const auto& [sigB, leafB] = *GetWinnerLeaf(b.second);
         return Stuff { leafA.dissimilaritySum / leafA.numComparisons,
                        leafA.bpmProbability } <
                Stuff { leafB.dissimilaritySum / leafB.numComparisons,
                        leafB.bpmProbability };
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
