#include "WavMirAudioSource.h"

#include <exception>

namespace MIR
{
WavMirAudioSource::WavMirAudioSource(const std::string& filename)
{
   WavFileIO::Info info;
   std::vector<std::vector<float>> samples;
   if (!WavFileIO::Read(filename, samples, info))
      throw std::runtime_error("Failed to read WAV file");

   const_cast<double&>(mSampleRate) = info.sampleRate;
   auto& mutableSamples = const_cast<std::vector<float>&>(mSamples);
   mutableSamples.resize(info.numFrames);
   if (info.numChannels == 2)
      for (size_t i = 0; i < info.numFrames; ++i)
         mutableSamples[i] = (samples[0][i] + samples[1][i]) / 2.f;
   else
      std::copy(samples[0].begin(), samples[0].end(), mutableSamples.begin());
}

int WavMirAudioSource::GetSampleRate() const
{
   return mSampleRate;
}

size_t WavMirAudioSource::ReadFloats(
   float* buffer, long long start, size_t numFrames) const
{
   const auto end = std::min<long long>(start + numFrames, mSamples.size());
   const auto numToRead = end - start;
   if (numToRead <= 0)
      return 0;
   std::copy(mSamples.begin() + start, mSamples.begin() + end, buffer);
   return numFrames;
}
} // namespace MIR
