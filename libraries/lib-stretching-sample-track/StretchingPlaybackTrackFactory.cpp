#include "StretchingPlaybackTrackFactory.h"

#include "StretchingSampleTrack.h"
#include "WaveTrack.h"

std::function<ConstSampleTrackHolder(SampleTrackHolder)>
StretchingPlaybackTrackFactory::GetStretchingSampleTrackFactory()
{
   return [](SampleTrackHolder sampleTrack) -> ConstSampleTrackHolder {
      if (track_cast<WaveTrack*>(sampleTrack.get()))
      {
         // todo(mhodgkinson) wrap into StretchingSampleTrack
         return std::make_shared<StretchingSampleTrack>(
            std::static_pointer_cast<WaveTrack>(sampleTrack));
      }
      else
      {
         return sampleTrack;
      }
   };
}
