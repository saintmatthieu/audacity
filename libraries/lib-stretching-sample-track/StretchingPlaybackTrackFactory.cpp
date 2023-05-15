#include "StretchingPlaybackTrackFactory.h"

#include "WaveTrack.h"

std::function<ConstSampleTrackHolder(ConstSampleTrackHolder)>
StretchingPlaybackTrackFactory::GetStretchingSampleTrackFactory()
{
   return [](ConstSampleTrackHolder sampleTrack) {
      if (track_cast<const WaveTrack*>(sampleTrack.get()))
      {
         // todo(mhodgkinson) wrap into StretchingSampleTrack
         return sampleTrack;
      }
      else
      {
         return sampleTrack;
      }
   };
}
