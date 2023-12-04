#include "WavMirAudioSource.h"

#include <cassert>
#include <exception>

namespace MIR
{
WavMirAudioSource::WavMirAudioSource(
   const std::string& filename, std::optional<double> timeLimit)
{
   WavFileIO::Info info;
   std::vector<std::vector<float>> samples;
   if (!WavFileIO::Read(filename, samples, info))
      throw std::runtime_error("Failed to read WAV file");

   const_cast<double&>(mSampleRate) = info.sampleRate;
   const auto limit = timeLimit.has_value() ?
                         static_cast<long long>(*timeLimit * info.sampleRate) :
                         std::numeric_limits<long long>::max();
   auto& mutableSamples = const_cast<std::vector<float>&>(mSamples);
   const auto numFrames = std::min<long long>(info.numFrames, limit);
   mutableSamples.resize(numFrames);
   if (info.numChannels == 2)
      for (size_t i = 0; i < numFrames; ++i)
         mutableSamples[i] = (samples[0][i] + samples[1][i]) / 2.f;
   else
      std::copy(
         samples[0].begin(), samples[0].begin() + numFrames,
         mutableSamples.begin());
}

int WavMirAudioSource::GetSampleRate() const
{
   return mSampleRate;
}

long long WavMirAudioSource::GetNumSamples() const
{
   return mSamples.size();
}

size_t WavMirAudioSource::ReadFloats(
   float* buffer, long long start, size_t numFrames, bool wrapAround) const
{
   if (wrapAround)
   {
      assert(mSamples.size() >= numFrames);
      while (start < 0)
         start += mSamples.size();
      while (start >= mSamples.size())
         start -= static_cast<long long>(mSamples.size());
   }
   const auto end = std::min<long long>(start + numFrames, mSamples.size());
   const auto numToRead = end - start;
   if (numToRead <= 0 && !wrapAround)
      return 0;
   std::copy(mSamples.begin() + start, mSamples.begin() + end, buffer);
   if (wrapAround)
   {
      const auto remainingToRead = numFrames - numToRead;
      std::copy(mSamples.begin() + end, mSamples.end(), buffer + numToRead);
      return numFrames;
   }
   else
      return numToRead;
}
} // namespace MIR
