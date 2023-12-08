#pragma once

#include <vector>

namespace MIR
{
class MirAudioSource;

class StftFrameProvider
{
public:
   StftFrameProvider(const MirAudioSource& source);
   bool GetNextFrame(std::vector<float>& frame);
   int GetSampleRate() const;
   double GetFrameRate() const;
   int GetFftSize() const;

private:
   const MirAudioSource& mSource;
   const int mFftSize;
   const int mHopSize;
   const std::vector<float> mWindow;
   const int mNumFrames;
   const long long mNumSamples;
   int mNumFramesProvided = 0;
};
} // namespace MIR
