#pragma once

#include <vector>

struct TIME_AND_PITCH_API AudioContainer
{
   AudioContainer(int numSamplesPerChannel, int numChannels);
   float* const* Get();
   std::vector<std::vector<float>> vectorVector;
   std::vector<float*> pointerVector;
};
