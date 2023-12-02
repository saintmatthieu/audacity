#include "QueenMaryBeatFinder.h"
#include "MirAudioSource.h"

#include <BarBeatTrack.h>
#include <algorithm>
#include <array>
#include <numeric>

namespace MIR
{
namespace
{
void concat(Vamp::Plugin::FeatureSet& a, const Vamp::Plugin::FeatureSet& b)
{
   for (const auto& [key, vectorToAppend] : b)
   {
      if (a.count(key) == 0)
         a[key] = {};
      auto& vector = a.at(key);
      vector.insert(vector.end(), vectorToAppend.begin(), vectorToAppend.end());
   }
}
} // namespace

QueenMaryBeatFinder::QueenMaryBeatFinder(const MirAudioSource& source)
{
   const auto sampleRate = source.GetSampleRate();
   BarBeatTracker plugin(sampleRate);
   const auto blockSize = plugin.getPreferredBlockSize();
   const auto stepSize = plugin.getPreferredStepSize();
   constexpr auto numChannels = 1;
   if (!plugin.initialise(numChannels, stepSize, blockSize))
      return;
   long start = 0;
   std::vector<float> buffer(blockSize);
   std::vector<float*> bufferPtr { buffer.data() };
   Vamp::Plugin::FeatureSet features;
   while (source.ReadFloats(buffer.data(), start, blockSize) == blockSize)
   {
      const auto timestamp = Vamp::RealTime::frame2RealTime(
         start, (int)(source.GetSampleRate() + 0.5));
      const auto newFeatures = plugin.process(bufferPtr.data(), timestamp);
      concat(features, newFeatures);
      start += stepSize;
   }
   concat(features, plugin.getRemainingFeatures());
   if (features.empty())
      return;
   std::vector<double> beatTimes;
   std::transform(
      features.at(0).begin(), features.at(0).end(),
      std::back_inserter(beatTimes), [](const Vamp::Plugin::Feature& feature) {
         return feature.timestamp.sec + feature.timestamp.nsec / 1e9;
      });
   const auto indexOfFirstDetectedBeat =
      std::stoi(features.at(0).at(0).label) - 1;
   const_cast<std::optional<BeatInfo>&>(
      mBeatInfo) = { beatTimes, indexOfFirstDetectedBeat };
   if (const auto odf = plugin.getDetectionFunctionOutput())
   {
      const_cast<std::vector<double>&>(mOnsetDetectionFunction) = *odf;
      const_cast<double&>(mOnsetDetectionFunctionSampleRate) =
         static_cast<double>(sampleRate) / stepSize;
   }
}

std::optional<BeatInfo> QueenMaryBeatFinder::GetBeats() const
{
   return mBeatInfo;
}

std::vector<double> QueenMaryBeatFinder::GetOnsetDetectionFunction() const
{
   return mOnsetDetectionFunction;
}

double QueenMaryBeatFinder::GetOnsetDetectionFunctionSampleRate() const
{
   return mOnsetDetectionFunctionSampleRate;
}
} // namespace MIR
