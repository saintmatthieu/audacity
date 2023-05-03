#include "WaveClip.h"
#include "WaveClipProcessor.h"

AudioSegmentProcessor& WaveClip::GetProcessor() const
{
   return WaveClipProcessor::Get(*this);
}
