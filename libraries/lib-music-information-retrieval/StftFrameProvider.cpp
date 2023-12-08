#include "StftFrameProvider.h"
#include "MirAudioSource.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numeric>

namespace MIR
{
namespace
{
constexpr auto twoPi = 2 * 3.14159265358979323846;

int GetFrameSize(int sampleRate)
{
   // 2048 frame size for sample rate 44.1kHz
   return 1 << (11 + (int)std::round(std::log2(sampleRate / 44100.)));
}
} // namespace

StftFrameProvider::StftFrameProvider(const MirAudioSource& source)
    : mSource { source }
    , mFftSize { GetFrameSize(source.GetSampleRate()) }
    , mHopSize { mFftSize / 4 }
    , mWindow { [this]() {
       std::vector<float> window(mFftSize);
       for (int i = 0; i < mFftSize; ++i)
          window[i] = 0.5 * (1 - std::cos(twoPi * i / mFftSize));
       const auto sum = std::accumulate(window.begin(), window.end(), 0.);
       std::transform(
          window.begin(), window.end(), window.begin(),
          [sum](float x) { return x / sum; });
       return window;
    }() }
    , mNumFrames { static_cast<int>(
         std::ceil(1. * source.GetNumSamples() / mHopSize)) }
    , mNumSamples { source.GetNumSamples() }
{
}

bool StftFrameProvider::GetNextFrame(std::vector<float>& frame)
{
   if (mNumFramesProvided >= mNumFrames)
      return false;
   frame.resize(mFftSize);
   const auto firstReadPosition = mHopSize - mFftSize;
   auto start = firstReadPosition + mNumFramesProvided * mHopSize;
   while (start < 0)
      start += mNumSamples;
   const auto end = std::min<long long>(start + mFftSize, mNumSamples);
   const auto numToRead = end - start;
   mSource.ReadFloats(frame.data(), start, numToRead);
   // It's not impossible that some user drops a file so short that `mFftSize >
   // mNumSamples`. In that case we won't be returning a meaningful
   // STFT, but that's a use case we're not interested in. We just need to make
   // sure we don't crash.
   const auto numRemaining = std::min(mFftSize - numToRead, mNumSamples);
   if (numRemaining >= 0)
      mSource.ReadFloats(frame.data() + numToRead, 0, numRemaining);
   std::transform(
      frame.begin(), frame.end(), mWindow.begin(), frame.begin(),
      std::multiplies<float>());
   ++mNumFramesProvided;
   return true;
}

int StftFrameProvider::GetSampleRate() const
{
   return mSource.GetSampleRate();
}

double StftFrameProvider::GetFrameRate() const
{
   return 1. * mSource.GetSampleRate() / mHopSize;
}

int StftFrameProvider::GetFftSize() const
{
   return mFftSize;
}
} // namespace MIR
