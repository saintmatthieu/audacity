#include "AudioContainer.h"

AudioContainer::AudioContainer(int numSamplesPerChannel, int numChannels)
    : _numChannels(numChannels)
{
   for (auto i = 0; i < numChannels; ++i)
   {
      std::vector<float> owner(numSamplesPerChannel);
      _input.push_back(owner.data());
      _inputOwner.emplace_back(std::move(owner));
   }
}

float* const* AudioContainer::get()
{
   return _input.data();
}

float* const* AudioContainer::getWithOffset(int offset)
{
   auto withOffset = _input.data();
   for (auto i = 0u; i < _numChannels; ++i)
   {
      withOffset[i] += offset;
   }
   return withOffset;
}