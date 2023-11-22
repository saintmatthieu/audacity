#include "GetBeatsUsingQueenMaryBarBeatTracking.h"
#include "CreateVampPluginInstance.h"
#include "MirAudioSource.h"

#include <algorithm>
#include <cassert>

namespace MIR
{
namespace GetBeatsUsingQueenMaryBarBeatTracking
{
std::optional<BeatInfo> GetBeats(const MirAudioSource& source)
{
   const auto sampleRate = source.GetSampleRate();
   const auto ibt =
      GetVampPluginInstance("qm-vamp-plugins:qm-barbeattracker", sampleRate);
   if (!ibt)
      return {};
   const auto outputDescriptors = ibt->getOutputDescriptors();
   const auto parameterDescriptors = ibt->getParameterDescriptors();
   const auto blockSize = ibt->getPreferredBlockSize();
   const auto stepSize = ibt->getPreferredStepSize();
   constexpr auto numChannels = 1;
   const auto initialized = ibt->initialise(numChannels, stepSize, blockSize);
   assert(initialized);
   if (!initialized)
      return {};
   long start = 0;
   std::vector<float> buffer(blockSize);
   std::vector<float*> bufferPtr { buffer.data() };
   while (source.ReadFloats(buffer.data(), start, blockSize) == blockSize)
   {
      const auto timestamp =
         Vamp::RealTime::frame2RealTime(start, (int)(sampleRate + 0.5));
      // Don't use output for this particular VAMP since it's acausal.
      ibt->process(bufferPtr.data(), timestamp);
      start += stepSize;
   }
   const auto features = ibt->getRemainingFeatures();
   if (features.empty())
      return {};
   // Find median of BPMs, taking average of middle two if the count is even:
   std::vector<double> beatTimes;
   std::transform(
      features.at(0).begin(), features.at(0).end(),
      std::back_inserter(beatTimes), [](const Vamp::Plugin::Feature& feature) {
         return feature.timestamp.sec + feature.timestamp.nsec / 1e9;
      });

   const auto indexOfFirstDetectedBeat =
      std::stoi(features.at(0).at(0).label) - 1;

   return { { beatTimes, indexOfFirstDetectedBeat } };
}
} // namespace GetBeatsUsingQueenMaryBarBeatTracking
} // namespace MIR
