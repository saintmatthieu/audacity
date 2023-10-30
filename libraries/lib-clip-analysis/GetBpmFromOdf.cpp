#include "GetBpmFromOdf.h"

#include "ClipAnalysisUtils.h"
#include "ClipInterface.h"
#include "FFT.h"
#include "GetBeatIndices.h"
#include "ODF.h"
#include "OnsetDetector.h"
#include "TimeDiv.h"

using IndexIndex = fluent::NamedType<size_t, struct IndexIndexTag>;
using II = IndexIndex;

#include <array>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <tuple>

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

void FillTree(
   const ODF& odf, const std::vector<size_t>& beatIndices, Tree& tree)
{
   const auto numBeats = beatIndices.size();
   std::vector<double> beatValues(numBeats);
   std::transform(
      beatIndices.begin(), beatIndices.end(), beatValues.begin(),
      [&](const size_t& i) { return odf.values[i]; });
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

struct Likelihood
{
   const double dissimilarity;
   const double bpmProbability;
};

bool operator<(const Likelihood& a, const Likelihood& b)
{
   const auto bpmA = std::log(a.bpmProbability);
   const auto bpmB = std::log(b.bpmProbability);
   const auto disA = std::log(a.dissimilarity);
   const auto disB = std::log(b.dissimilarity);

   // TODO tune
   const auto disWeight = 1.;
   return bpmA - disA * disWeight < bpmB - disB * disWeight;
}

auto GetWinnerLeaf(const Branch& branch)
{
   return std::max_element(
      branch.begin(), branch.end(),
      [](const std::pair<TimeSignature, Leaf>& a,
         const std::pair<TimeSignature, Leaf>& b) {
         return Likelihood {
            a.second.dissimilaritySum / a.second.numComparisons,
            a.second.bpmProbability
         } < Likelihood { b.second.dissimilaritySum / b.second.numComparisons,
                          b.second.bpmProbability };
      });
}

const std::unordered_map<TimeSignature, std::vector<Level>> barBeatLevels {
   { TimeSignature::FourFour, { L { 0 }, L { 2 }, L { 1 }, L { 2 } } },
   { TimeSignature::ThreeFour, { L { 0 }, L { 1 }, L { 1 } } },
   { TimeSignature::SixEight,
     { L { 0 }, L { 2 }, L { 2 }, L { 1 }, L { 2 }, L { 2 } } }
};

std::vector<Level> GetDivisionLevels(int numBars, TimeSignature timeSignature)
{
   const auto& strengths = barBeatLevels.at(timeSignature);
   // Replicate for each bar.
   std::vector<Level> result;
   result.reserve(numBars * strengths.size());
   for (auto i = 0; i < numBars; ++i)
      result.insert(result.end(), strengths.begin(), strengths.end());
   return result;
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
      {
         const auto i = p1 * beatsPerPeriod;
         const auto j = p2 * beatsPerPeriod;
         if (beatRecursionLevels[i] == beatRecursionLevels[j])
            pairs.emplace_back(i, j);
      }

   return pairs;
}

std::optional<std::vector<Level>>
GetDivisionLevels(const DivisionLevelInput& input)
{
   const auto valid = input.totalNumDivs > 0 && input.numBars > 0 &&
                      input.timeSignature != TimeSignature::_Count;
   assert(valid);
   if (!valid)
      return std::nullopt;
   const auto strengths = GetDivisionLevels(input.numBars, input.timeSignature);
   if (input.totalNumDivs % strengths.size() != 0)
      return std::nullopt;
   const auto tatumStrength =
      *std::max_element(strengths.begin(), strengths.end()) + Level { 1 };
   std::vector<Level> result(input.totalNumDivs);
   const auto step = input.totalNumDivs / strengths.size();
   for (auto i = 0; i < input.totalNumDivs; ++i)
      result[i] = i % step == 0 ? strengths[i / step] : tatumStrength;
   return result;
}

std::vector<Ordinal> ToSeriesOrdinals(const std::vector<Level>& divisionLevels)
{
   std::vector<Ordinal> ordinals(divisionLevels.size());
   // Sometimes I think only events of equal ordinality should be compared,
   // sometimes not ... This is to account for the fact that a ternary
   // subdivision may not have all sub-beats equally accentuated. Example of a
   // swung rhythm, with level hierarchy `0 1 1 0 1 1 ...`. The first `1` of
   // each pair is in general not accentuated, leading to a pattern `a b c a b
   // c`, where `a` is the downbeat, `c` the swung subbeat, and `b` is silence.
   // In such case, comparing `b`s with `c`s would be misleading.
   constexpr auto ignoreThis = false;
   if (ignoreThis)
      return ordinals;
   // TODO there is a better way: use `std::pair<std::vector<size_t>,
   // std::vector<size_t>>`. For binary subdivisions, the vectors have size one,
   // but with ternary subdivisions, the vectors have size two.
   std::map<Level, Ordinal> levelOrdinals;
   for (auto l = 0; l < divisionLevels.size(); ++l)
   {
      const auto divLevel = divisionLevels[l];
      // Reset counters of all levels higher than the current one.
      for (auto& [level, ordinal] : levelOrdinals)
         if (level > divLevel)
            ordinal = O { 0 };
      if (!levelOrdinals.count(divLevel))
         levelOrdinals[divLevel] = O { 0 };
      ordinals[l] = l == 0 ? O { 0 } : levelOrdinals.at(divLevel)++;
   }
   return ordinals;
}

std::optional<Result> GetBpmFromOdf(const ODF& odf)
{
   Tree tree;
   const auto beatIndices = GetBeatIndices(odf);
   if (!beatIndices.has_value())
      return {};
   FillTree(odf, *beatIndices, tree);
   if (tree.empty())
      return {};
   const auto& [numBars, branch] = *std::max_element(
      tree.begin(), tree.end(),
      [](const std::pair<int, Branch>& a, const std::pair<int, Branch>& b) {
         const auto& [sigA, leafA] = *GetWinnerLeaf(a.second);
         const auto& [sigB, leafB] = *GetWinnerLeaf(b.second);
         return Likelihood { leafA.dissimilaritySum / leafA.numComparisons,
                             leafA.bpmProbability } <
                Likelihood { leafB.dissimilaritySum / leafB.numComparisons,
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

namespace
{
std::vector<IndexIndex> FilterByLevel(
   size_t numBeats, const std::vector<Level>& beatLevels, Level level)
{
   assert(numBeats == beatLevels.size());
   std::vector<II> result;
   for (auto i = 0u; i < numBeats; ++i)
      if (beatLevels[i] == level)
         result.push_back(II { i });
   return result;
}

std::vector<std::pair<II, II>> GetUniqueCombinations(
   const std::vector<II>& indices, const std::vector<Ordinal>& seriesOrdinals)
{
   std::vector<std::pair<II, II>> pairs;
   for (auto i = 0; i < indices.size() - 1; ++i)
      for (auto j = i + 1; j < indices.size(); ++j)
         if (
            seriesOrdinals[indices[i].get()] ==
            seriesOrdinals[indices[j].get()])
            pairs.emplace_back(indices[i], indices[j]);
   return pairs;
}

std::vector<double>
ToLog(const std::vector<double>& values, const std::vector<size_t>& indices)
{
   std::vector<double> result(indices.size());
   std::transform(
      indices.begin(), indices.end(), result.begin(),
      [&](size_t i) { return std::log(values[i]); });
   return result;
}

struct Comparison
{
   size_t i;
   size_t j;
   double d;
   double w;
};

struct Hypothesis
{
   TimeSignature sig = TimeSignature::_Count;
   int numBars = 0;
   double dissimilarity = 0.;
   double bpmProbability = 0.;
   std::vector<Comparison> comparisons;
};
} // namespace

std::optional<Result> GetBpmFromOdf2(const ODF& odf)
{
   const auto beatIndices = GetBeatIndices(odf);
   if (!beatIndices.has_value())
      return {};

   const int numDivisions = beatIndices->size();
   const auto logValues = ToLog(odf.values, *beatIndices);
   std::vector<Hypothesis> hypotheses;
   for (auto numBars = 1; numBars <= 8; ++numBars)
      for (auto t = 0; t < static_cast<int>(TimeSignature::_Count); ++t)
      {
         const auto sig = static_cast<TimeSignature>(t);
         const auto divLevels = GetDivisionLevels(
            DivisionLevelInput { numDivisions, numBars, sig });
         if (!divLevels.has_value())
            continue;
         const auto seriesOrdinals = ToSeriesOrdinals(*divLevels);
         const auto levelSet =
            std::set<Level> { divLevels->begin(), divLevels->end() };

         std::vector<Comparison> comparisons;
         for (auto level : levelSet)
         {
            const std::vector<IndexIndex> levelMatchingIndices =
               FilterByLevel(numDivisions, *divLevels, level);
            const auto iiPairs =
               GetUniqueCombinations(levelMatchingIndices, seriesOrdinals);
            std::transform(
               iiPairs.begin(), iiPairs.end(), std::back_inserter(comparisons),
               [&](const std::pair<II, II>& p) {
                  const auto i = p.first.get();
                  const auto j = p.second.get();
                  const auto diff = logValues[i] - logValues[j];
                  const double weight = 1. / (1 << level.get());
                  return Comparison { (*beatIndices)[i], (*beatIndices)[j],
                                      diff, weight };
               });
         }
         const auto weigthSum = std::accumulate(
            comparisons.begin(), comparisons.end(), 0.,
            [](double sum, const Comparison& c) { return sum + c.w; });
         const auto dissimilaritySum = std::accumulate(
            comparisons.begin(), comparisons.end(), 0.,
            [](double sum, const Comparison& c) {
               return sum + (c.d < 0 ? -c.d : c.d) * c.w;
            });
         const auto dissimilarity = dissimilaritySum / weigthSum;
         const auto bpmProb = GetBpmProbability(sig, numBars, odf.duration);
         hypotheses.emplace_back(
            Hypothesis { sig, numBars, dissimilarity, bpmProb, comparisons });
      }

   if (hypotheses.empty())
      return {};

   std::sort(
      hypotheses.begin(), hypotheses.end(),
      [](const Hypothesis& a, const Hypothesis& b) {
         return !(
            Likelihood { a.dissimilarity, a.bpmProbability } <
            Likelihood { b.dissimilarity, b.bpmProbability });
      });
   const auto& winner = hypotheses.front();
   const auto barsPerMinute = winner.numBars * 60 / odf.duration;
   const auto quarternotesPerMinute =
      // 6/8 two beats but three quarter notes per bar.
      winner.sig == TimeSignature::SixEight ?
         3 * barsPerMinute :
         beatsPerBar.at(winner.sig) * barsPerMinute;
   return Result { winner.numBars, quarternotesPerMinute, winner.sig };
}

} // namespace ClipAnalysis
