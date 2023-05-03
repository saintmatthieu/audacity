#pragma once

#include "AudioSegment.h"

class SilenceSegment : public AudioSegment
{
public:
   SilenceSegment(double duration);
   AudioSegmentProcessor& GetProcessor() const override;

private:
   const double mDuration;
};
