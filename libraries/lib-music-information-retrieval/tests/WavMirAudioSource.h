#pragma once

#include "MirAudioSource.h"

#include <WavFileIO.h>

namespace MIR
{
class WavMirAudioSource : public MirAudioSource
{
public:
   WavMirAudioSource(
      const std::string& filename, std::optional<double> timeLimit = {});

   int GetSampleRate() const override;
   long long GetNumSamples() const override;
   size_t ReadFloats(
      float* buffer, long long start, size_t numFrames,
      bool wrapAround) const override;

private:
   const std::vector<float> mSamples;
   const double mSampleRate = 0.;
};
} // namespace MIR
