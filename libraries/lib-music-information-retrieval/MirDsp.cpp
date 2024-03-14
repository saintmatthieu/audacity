/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  MirDsp.cpp

  Matthieu Hodgkinson

**********************************************************************/
#include "MirDsp.h"
#include "IteratorX.h"
#include "MemoryX.h"
#include "MirTypes.h"
#include "MirUtils.h"
#include "PowerSpectrumGetter.h"
#include "StftFrameProvider.h"

#include <array>
#include <cassert>
#include <cmath>
#include <numeric>
#include <pffft.h>
#include <unordered_set>

namespace MIR
{
namespace
{
constexpr float GetLog2(float x)
{
   // https://stackoverflow.com/a/28730362
   union
   {
      float val;
      int32_t x;
   } u = { x };
   auto log_2 = (float)(((u.x >> 23) & 255) - 128);
   u.x &= ~(255 << 23);
   u.x += 127 << 23;
   log_2 += ((-0.3358287811f) * u.val + 2.0f) * u.val - 0.65871759316667f;
   return log_2;
}

float GetNoveltyMeasure(
   const std::vector<float>& prevPowSpec, const std::vector<float>& powSpec)
{
   auto k = 0;
   const auto measure = std::accumulate(
      powSpec.begin(), powSpec.end(), 0.f, [&](float a, float mag) {
         // Half-wave-rectified stuff
         return a + std::max(0.f, mag - prevPowSpec[k++]);
      });
   return measure / powSpec.size() > odfNoiseGate ? measure : 0.f;
}

// Function to assign each point to the closest centroid
void AssignToClusters(
   const std::vector<float>& data, const std::vector<float>& centroids,
   std::vector<size_t>& clusters)
{
   const auto K = centroids.size();
   for (auto i = 0; i < data.size(); ++i)
   {
      float minDist = std::numeric_limits<float>::max();
      int clusterIdx = 0;
      for (auto k = 0; k < K; ++k)
      {
         const auto dist = std::abs(data[i] - centroids[k]);
         if (dist < minDist)
         {
            minDist = dist;
            clusterIdx = k;
         }
      }
      clusters[i] = clusterIdx;
   }
}

// Function to update centroids based on current cluster assignments
void UpdateCentroids(
   const std::vector<float>& data, const std::vector<size_t>& clusters,
   std::vector<float>& centroids)
{
   const auto K = centroids.size();
   std::vector<int> counts(K, 0);
   std::vector<float> sums(K, 0.f);

   for (auto i = 0; i < data.size(); ++i)
   {
      int clusterIdx = clusters[i];
      sums[clusterIdx] += data[i];
      counts[clusterIdx]++;
   }

   for (auto k = 0; k < K; ++k)
      if (counts[k] > 0)
         centroids[k] = sums[k] / counts[k];
}

constexpr auto GetSquaredError(float a, float b)
{
   return (a - b) * (a - b);
}

void RemoveNoisePeaksByLevel(
   std::vector<float>& odf, QuantizationFitDebugOutput* debugOutput)
{
   const auto peakIndices = GetPeakIndices(odf);
   const auto peakValues = [&] {
      std::vector<float> peakValues;
      peakValues.reserve(peakIndices.size());
      for (auto i : peakIndices)
         peakValues.push_back(odf[i]);
      return peakValues;
   }();

   const auto clusters = GetClusters(peakValues);
   assert(clusters.size() == 3); // TODO use array<Cluster, 3> instead of vector
   if (debugOutput)
      debugOutput->clusters = clusters;

   const auto peaksToRemove = [&] {
      // Remove clusters with too high peak rates ; these are likely to be
      // noise.
      const auto centroids = [&] {
         std::vector<float> centroids;
         centroids.reserve(3);
         for (const auto& cluster : clusters)
         {
            const auto sum = std::accumulate(
               cluster.begin(), cluster.end(), 0.f,
               [&](float sum, size_t i) { return sum + peakValues[i]; });
            centroids.push_back(sum / cluster.size());
         }
         return centroids;
      }();
      const auto d1 = centroids[1] - centroids[0];
      const auto d2 = centroids[2] - centroids[1];
      std::vector<size_t> peaksToRemove = clusters[0];
      if (debugOutput)
         debugOutput->suppressedClusters.push_back(0);
      if (d2 > 4 * d1)
      {
         peaksToRemove.insert(
            peaksToRemove.end(), clusters[1].begin(), clusters[1].end());
         if (debugOutput)
            debugOutput->suppressedClusters.push_back(1);
      }
      return peaksToRemove;
   }();

   const auto troughIndices = [&] {
      std::unordered_set<int> troughIndexSet;
      const auto N = odf.size();
      std::for_each(peakIndices.begin(), peakIndices.end(), [&](int peakIndex) {
         int i = peakIndex - 1;
         // Decrement i until we find a trough. We wrap around readout, but not
         // the index just yet.
         while (odf[(i + N - 1) % N] <= odf[(i + N) % N])
            --i;
         troughIndexSet.insert(i);
         // Do the same in the positive direction:
         i = peakIndex + 1;
         while (odf[(i + 1) % N] <= odf[(i) % N])
            ++i;
         troughIndexSet.insert(i);
      });
      std::vector<int> troughIndices(
         troughIndexSet.begin(), troughIndexSet.end());
      std::sort(troughIndices.begin(), troughIndices.end());
      return troughIndices;
   }();

   auto rightTroughIndicesIndex = 1;
   std::for_each(
      peaksToRemove.begin(), peaksToRemove.end(), [&](size_t peakIndicesIndex) {
         const auto indexOfPeakToRemove = peakIndices[peakIndicesIndex];
         while (indexOfPeakToRemove > troughIndices[rightTroughIndicesIndex])
            ++rightTroughIndicesIndex;
         const auto leftTroughIndicesIndex = rightTroughIndicesIndex - 1;

         // Now we wrap around.
         const auto leftTroughIndex =
            (troughIndices[leftTroughIndicesIndex] + odf.size()) % odf.size();
         const auto rightTroughIndex =
            troughIndices[rightTroughIndicesIndex] % odf.size();

         if (leftTroughIndex > rightTroughIndex)
         {
            std::fill(odf.begin() + leftTroughIndex, odf.end(), 0.f);
            std::fill(odf.begin(), odf.begin() + rightTroughIndex, 0.f);
         }
         else
            std::fill(
               odf.begin() + leftTroughIndex, odf.begin() + rightTroughIndex,
               0.f);
      });
}

void SimulateTemporalMasking(std::vector<float>& odf, double odfSr)
{
   const auto peakIndices = GetPeakIndices(odf);
   if (peakIndices.size() < 2)
      return;
   // Simulate auditory temporal masking, whereby large onsets mask smaller
   // subsequent onsets for a short time. We also do this in the reverse
   // direction.
   constexpr auto timeWindow = 0.1;
   const auto windowSize =
      std::min<size_t>(timeWindow * odfSr + .5, odf.size());
   std::unordered_set<size_t> shadowedIndexSet;
   for (int ii = 0; ii < peakIndices.size(); ++ii)
   {
      const auto peakIndex = peakIndices[ii];
      const auto peakValue = odf[peakIndex];
      for (auto j = 1; j < windowSize; ++j)
      {
         const auto scalar = 1 - j / windowSize;
         const auto i = (peakIndex - j + odf.size()) % odf.size();
         if (odf[i] < peakValue * scalar)
            shadowedIndexSet.insert(i);
         const auto k = (peakIndex + j + 1) % odf.size();
         if (odf[k] < peakValue * scalar)
            shadowedIndexSet.insert(k);
      }
   }
   // To help debugging.
   std::vector<size_t> shadowedIndices(
      shadowedIndexSet.begin(), shadowedIndexSet.end());
   std::sort(shadowedIndices.begin(), shadowedIndices.end());
   std::for_each(shadowedIndices.begin(), shadowedIndices.end(), [&](size_t i) {
      odf[i] = 0.f;
   });
}
} // namespace

std::vector<Cluster> GetClusters(const std::vector<float>& x)
{
   const auto avg = std::accumulate(x.begin(), x.end(), 0.f) / x.size();
   constexpr auto K = 3;

   std::vector<float> centroids(K);
   // Initialize the centroids around avg:
   for (auto k = 0; k < K; ++k)
      centroids[k] = avg + (k - K / 2) * 0.1f;

   std::vector<size_t> assignments(x.size());
   std::vector<size_t> oldAssignments(x.size(), -1);
   for (auto iter = 0; iter < 100; ++iter)
   {
      AssignToClusters(x, centroids, assignments);
      if (oldAssignments == assignments)
         break;
      UpdateCentroids(x, assignments, centroids);
      std::swap(oldAssignments, assignments);
   }
   std::swap(assignments, oldAssignments);

   std::vector<Cluster> clusters;
   for (auto k = 0; k < K; ++k)
   {
      std::vector<size_t> cluster;
      for (auto i = 0; i < x.size(); ++i)
         if (assignments[i] == k)
            cluster.push_back(i);
      clusters.push_back(cluster);
   }

   return clusters;
}

std::vector<float> GetMovingAverage(const std::vector<float>& x, double hopRate)
{
   constexpr auto smoothingWindowDuration = 0.2;
   // An odd number.
   const int M = std::round(smoothingWindowDuration * hopRate / 4) * 2 + 1;
   const auto window = GetNormalizedHann(2 * M + 1);
   auto n = 0;
   std::vector<float> movingAverage(x.size());
   std::transform(x.begin(), x.end(), movingAverage.begin(), [&](float) {
      const auto m = IotaRange(-M, M + 1);
      const auto y =
         std::accumulate(m.begin(), m.end(), 0.f, [&](float y, int i) {
            auto k = n + i;
            while (k < 0)
               k += x.size();
            while (k >= x.size())
               k -= x.size();
            return y + x[k] * window[i + M];
         });
      ++n;
      return y;
   });
   return movingAverage;
}

std::vector<float> GetNormalizedCircularAutocorr(std::vector<float> x)
{
   if (std::all_of(x.begin(), x.end(), [](float x) { return x == 0.f; }))
      return x;
   const auto N = x.size();
   assert(IsPowOfTwo(N));
   PFFFT_Setup* setup = pffft_new_setup(N, PFFFT_REAL);
   Finally Do { [&] { pffft_destroy_setup(setup); } };
   std::vector<float> work(N);
   pffft_transform_ordered(
      setup, x.data(), x.data(), work.data(), PFFFT_FORWARD);

   // Transform to a power spectrum, but preserving the layout expected by PFFFT
   // in preparation for the inverse transform.
   x[0] *= x[0];
   x[1] *= x[1];
   for (auto n = 2; n < N; n += 2)
   {
      x[n] = x[n] * x[n] + x[n + 1] * x[n + 1];
      x[n + 1] = 0.f;
   }

   pffft_transform_ordered(
      setup, x.data(), x.data(), work.data(), PFFFT_BACKWARD);

   // The second half of the circular autocorrelation is the mirror of the first
   // half. We are economic and only keep the first half.
   x.erase(x.begin() + N / 2 + 1, x.end());

   const auto normalizer = 1 / x[0];
   std::transform(x.begin(), x.end(), x.begin(), [normalizer](float x) {
      return x * normalizer;
   });
   return x;
}

std::vector<float> GetOnsetDetectionFunction(
   const MirAudioReader& audio,
   const std::function<void(double)>& progressCallback,
   QuantizationFitDebugOutput* debugOutput)
{
   StftFrameProvider frameProvider { audio };
   const auto sampleRate = frameProvider.GetSampleRate();
   const auto numFrames = frameProvider.GetNumFrames();
   const auto frameSize = frameProvider.GetFftSize();
   std::vector<float> buffer(frameSize);
   std::vector<float> odf;
   odf.reserve(numFrames);
   const auto powSpecSize = frameSize / 2 + 1;
   std::vector<float> powSpec(powSpecSize);
   std::vector<float> prevPowSpec(powSpecSize);
   std::vector<float> firstPowSpec;
   std::fill(prevPowSpec.begin(), prevPowSpec.end(), 0.f);

   PowerSpectrumGetter getPowerSpectrum { frameSize };

   auto frameCounter = 0;
   while (frameProvider.GetNextFrame(buffer))
   {
      getPowerSpectrum(buffer.data(), powSpec.data());

      // Compress the frame as per section (6.5) in Müller, Meinard.
      // Fundamentals of music processing: Audio, analysis, algorithms,
      // applications. Vol. 5. Cham: Springer, 2015.
      constexpr auto gamma = 1000.f;
      std::transform(
         powSpec.begin(), powSpec.end(), powSpec.begin(),
         [gamma](float x) { return GetLog2(1 + gamma * std::sqrt(x)); });

      if (firstPowSpec.empty())
         firstPowSpec = powSpec;
      else
         odf.push_back(GetNoveltyMeasure(prevPowSpec, powSpec));

      if (debugOutput)
         debugOutput->postProcessedStft.push_back(powSpec);

      std::swap(prevPowSpec, powSpec);

      if (progressCallback)
         progressCallback(1. * ++frameCounter / numFrames);
   }

   // Close the loop.
   odf.push_back(GetNoveltyMeasure(prevPowSpec, firstPowSpec));
   assert(IsPowOfTwo(odf.size()));

   const auto movingAverage =
      GetMovingAverage(odf, frameProvider.GetFrameRate());

   // Subtract moving average from ODF.
   std::transform(
      odf.begin(), odf.end(), movingAverage.begin(), odf.begin(),
      [](float a, float b) { return std::max<float>(a - b, 0.f); });

   if (debugOutput)
   {
      debugOutput->rawOdf = odf;
      debugOutput->rawOdfPeakIndices = GetPeakIndices(odf);
   }

   RemoveNoisePeaksByLevel(odf, debugOutput);
   SimulateTemporalMasking(odf, frameProvider.GetFrameRate());

   return odf;
}
} // namespace MIR
