#pragma once

#include "WaveTrack.h"

class STRETCHING_SAMPLE_TRACK_API StretchingSampleTrack final :
    public SampleTrack
{
public:
   StretchingSampleTrack();

private:
   const std::shared_ptr<WaveTrack> mWaveTrack;
};
