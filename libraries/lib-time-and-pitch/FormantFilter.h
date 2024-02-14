#pragma once

#include <array>
#include <pffft.h>
#include <vector>

class TIME_AND_PITCH_API FormantFilter
{
public:
   FormantFilter(int sampleRate, size_t numChannels);
   ~FormantFilter();

   void Whiten(float* const* channels, size_t numSamples);
   void Color(float* const* channels, size_t numSamples);

private:
   void ProcessWindow();

   const size_t mNumChannels;
   const std::vector<float> mWindow;
   const size_t mHopSize;
   std::vector<std::vector<float>> mInBuffers;
   std::vector<std::vector<float>> mOutBuffers;

   PFFFT_Setup* mSetup;
   float* mWork;
   float* mTmp;
};
