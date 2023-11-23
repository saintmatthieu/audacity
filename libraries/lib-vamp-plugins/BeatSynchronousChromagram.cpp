#include "BeatSynchronousChromagram.h"

BeatSynchronousChromagram::BeatSynchronousChromagram(float inputSampleRate)
    : Vamp::Plugin(inputSampleRate)
{
}

bool BeatSynchronousChromagram::initialise(
   size_t inputChannels, size_t stepSize, size_t blockSize)
{
   return true;
}

void BeatSynchronousChromagram::reset()
{
}

BeatSynchronousChromagram::InputDomain
BeatSynchronousChromagram::getInputDomain() const
{
   return TimeDomain;
}

BeatSynchronousChromagram::OutputList
BeatSynchronousChromagram::getOutputDescriptors() const
{
   OutputList list;

   OutputDescriptor d;
   d.identifier = "beats";
   d.name = "Beats";
   d.description = "Beat locations";
   d.unit = "";
   d.hasFixedBinCount = true;
   d.binCount = 0;
   d.hasKnownExtents = false;
   d.isQuantized = false;
   d.sampleType = OutputDescriptor::VariableSampleRate;
   d.sampleRate = m_inputSampleRate;
   list.push_back(d);

   return list;
}

BeatSynchronousChromagram::FeatureSet BeatSynchronousChromagram::process(
   const float* const* inputBuffers, Vamp::RealTime timestamp)
{
   return {};
}

BeatSynchronousChromagram::FeatureSet
BeatSynchronousChromagram::getRemainingFeatures()
{
   return {};
}

std::string BeatSynchronousChromagram::getIdentifier() const
{
   return "my_vamp_plugin";
}

std::string BeatSynchronousChromagram::getName() const
{
   return "My Audacity Vamp Plugin";
}

std::string BeatSynchronousChromagram::getDescription() const
{
   return "test plugin";
}

std::string BeatSynchronousChromagram::getMaker() const
{
   return "Matthieu Hodgkinson";
}

std::string BeatSynchronousChromagram::getCopyright() const
{
   return "GPL3";
}

int BeatSynchronousChromagram::getPluginVersion() const
{
   return 0;
}
