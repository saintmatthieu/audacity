#pragma once

namespace MIR
{
class MirAudioSource
{
public:
   virtual int GetSampleRate() const = 0;
   virtual size_t
   ReadFloats(float* buffer, long long where, size_t numFrames) const = 0;

   virtual ~MirAudioSource() = default;
};
} // namespace MIR
