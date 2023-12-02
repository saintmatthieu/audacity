#include "BeatFinder.h"
#include "QueenMaryBeatFinder.h"

#include <cassert>

namespace MIR
{
std::unique_ptr<BeatFinder> BeatFinder::CreateInstance(
   const MirAudioSource& source, BeatTrackingAlgorithm algorithm)
{
   switch (algorithm)
   {
   case BeatTrackingAlgorithm::QueenMaryBarBeatTrack:
      return std::make_unique<QueenMaryBeatFinder>(source);
   case BeatTrackingAlgorithm::BTrack:
      return nullptr;
   default:
      assert(false);
      return nullptr;
   }
}

} // namespace MIR
