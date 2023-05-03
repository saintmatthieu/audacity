#pragma once

#include "AudioSegment.h"

class WaveClip : public AudioSegment
{
public:
   AudioSegmentProcessor& GetProcessor() const override;
};
