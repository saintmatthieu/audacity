#pragma once

#include <BTrack.h>
#include <vamp-sdk/Plugin.h>

class BeatSynchronousChromagram : public Vamp::Plugin
{
public:
   BeatSynchronousChromagram(float inputSampleRate);

   bool
   initialise(size_t inputChannels, size_t stepSize, size_t blockSize) override;

   void reset() override;

   InputDomain getInputDomain() const override;

   OutputList getOutputDescriptors() const override;

   FeatureSet
   process(const float* const* inputBuffers, Vamp::RealTime timestamp) override;

   FeatureSet getRemainingFeatures() override;

   std::string getIdentifier() const override;

   std::string getName() const override;

   std::string getDescription() const override;

   std::string getMaker() const override;

   std::string getCopyright() const override;

   int getPluginVersion() const override;

private:
   BTrack mBTrack;
};
