#include "GetBeats.h"
#include "GetBeatsUsingBTrack.h"
#include "GetBeatsUsingQueenMaryBarBeatTracking.h"

#include <cassert>

namespace MIR
{
std::optional<BeatInfo>
GetBeats(BeatTrackingAlgorithm algorithm, const MirAudioSource& source)
{
   switch (algorithm)
   {
   case BeatTrackingAlgorithm::QeenMaryBarBeatTrack:
      return GetBeatsUsingQueenMaryBarBeatTracking::GetBeats(source);
   case BeatTrackingAlgorithm::BTrack:
      return GetBeatsUsingBTrack::GetBeats(source);
   default:
      assert(false);
      return {};
   }
}
} // namespace MIR
