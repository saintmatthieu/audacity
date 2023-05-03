#pragma once

#include "AudioSegment.h"

class SilenceSegment : public AudioSegment
{
public:
   AudioSegmentProcessor& GetProcessor() const override;
};
