#include "WaveClipSegment.h"

static WaveClip::ProcessorCaches::RegisteredFactory sKeyS { [](WaveClip&) {
   return std::make_unique<WaveClipSegment>();
} };

WaveClipSegment& WaveClipSegment::Get(const WaveClip& clip)
{
   return const_cast<WaveClip&>(clip)
      .ProcessorCaches::Get<WaveClipSegment>(sKeyS);
}

void WaveClipSegment::SetReadPosition(double t)
{
}

size_t
WaveClipSegment::GetAudio(float* const* buffer, size_t bufferSize) const
{
   return 0;
}
