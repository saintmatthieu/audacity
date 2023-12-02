#include "GetBeatsUsingQueenMaryBarBeatTracking.h"

#include "GetVampFeaturesFromPlugin.h"
#include "MirAudioSource.h"
#include "QueenMaryBeatFinder.h"

#include <algorithm>
#include <cassert>

namespace MIR
{
namespace GetBeatsUsingQueenMaryBarBeatTracking
{
std::optional<BeatInfo> GetBeats(const MirAudioSource& source)
{
   const auto sampleRate = source.GetSampleRate();
   QueenMaryBeatFinder beatFinder(source);
   return beatFinder.GetBeats();
}
} // namespace GetBeatsUsingQueenMaryBarBeatTracking
} // namespace MIR
