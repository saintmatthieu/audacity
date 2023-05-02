#include "WaveTrackTimeStretcher.h"

static WaveTrack::AttachedObjects::RegisteredFactory key {
   [](WaveTrack& track) {
      return std::make_unique<WaveTrackTimeStretcher>(track);
   }
};

WaveTrackTimeStretcher& WaveTrackTimeStretcher::Get(WaveTrack& track)
{
   return track.AttachedObjects::Get<WaveTrackTimeStretcher>(key);
}

const WaveTrackTimeStretcher& WaveTrackTimeStretcher::Get(const WaveTrack& track)
{
   return Get(const_cast<WaveTrack&>(track));
}

WaveTrackTimeStretcher::WaveTrackTimeStretcher(WaveTrack& track)
    : mTrack(track)
{
}

void WaveTrackTimeStretcher::GetFloats(
   float* const* buffers, size_t samplesPerChannel)
{
}
