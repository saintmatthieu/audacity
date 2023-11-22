#include "GetBeats.h"
#include "GetBeatsUsingQueenMaryBarBeatTracking.h"

#include <cassert>

namespace MIR
{
std::optional<BeatInfo>
GetBeatInfo(BeatTrackingAlgorithm algorithm, const MirAudioSource& source)
{
   switch (algorithm)
   {
   case BeatTrackingAlgorithm::QeenMaryBarBeatTrack:
      return GetBeatsUsingQueenMaryBarBeatTracking::GetBeats(source);
   defautl:
      assert(false);
      return {};
   }
}
} // namespace MIR
