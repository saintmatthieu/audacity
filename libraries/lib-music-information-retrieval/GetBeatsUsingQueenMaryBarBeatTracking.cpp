#include "GetBeatsUsingQueenMaryBarBeatTracking.h"
#include "GetVampFeatures.h"
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
   const auto features =
      GetVampFeatures("qmvampplugins:qm-barbeattracker", source);
   if (features.empty())
      return {};
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
