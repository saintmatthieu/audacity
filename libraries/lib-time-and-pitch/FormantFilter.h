#pragma once

#include <array>
#include <vector>

class TIME_AND_PITCH_API FormantFilter
{
public:
   static constexpr size_t numFormants = 3;

   FormantFilter(int sampleRate, size_t numChannels);

   void Whiten(float* const* channels, size_t numSamples);
   void Color(float* const* channels, size_t numSamples);

private:
   void UpdateCoefficients();

   const size_t mNumChannels;
   const size_t mHopSize;
   size_t mCountSinceHop = 0;
   const std::vector<float> mWindow;
   std::array<double, numFormants> mLpcCoefs;
   std::array<double, numFormants> mOldLpcCoefs;
   std::vector<std::vector<float>> mBuffers;
   std::vector<std::array<double, numFormants>> mWhiteningStates;
   std::vector<std::array<double, numFormants>> mColoringStates;
};
