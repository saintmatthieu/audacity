#include "AudioContainer.h"

AudioContainer::AudioContainer(int numSamplesPerChannel, int numChannels)
{
   for (auto i = 0; i < numChannels; ++i)
   {
      std::vector<float> owner(numSamplesPerChannel);
      pointerVector.push_back(owner.data());
      vectorVector.emplace_back(std::move(owner));
   }
}

float* const* AudioContainer::Get()
{
   return pointerVector.data();
}
