#pragma once

namespace MIR
{
class MirAudioSource
{
public:
   virtual int GetSampleRate() const = 0;
   virtual long long GetNumSamples() const = 0;
   virtual void
   ReadFloats(float* buffer, long long where, size_t numFrames) const = 0;
   virtual ~MirAudioSource() = default;
};
} // namespace MIR
