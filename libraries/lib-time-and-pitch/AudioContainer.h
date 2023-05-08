#pragma once

#include <vector>

class TIME_AND_PITCH_API AudioContainer
{
public:
   AudioContainer(int numSamplesPerChannel, int numChannels);
   float* const* get();
   float* const* getWithOffset(int offset);

private:
   std::vector<std::vector<float>> _inputOwner;
   std::vector<float*> _input;
   const int _numChannels;
};
