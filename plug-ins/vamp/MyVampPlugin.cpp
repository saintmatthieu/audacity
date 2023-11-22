#include "MyVampPlugin.h"

MyVampPlugin::MyVampPlugin(float inputSampleRate)
    : Vamp::Plugin(inputSampleRate)
{
}

bool MyVampPlugin::initialise(
   size_t inputChannels, size_t stepSize, size_t blockSize)
{
   return true;
}

void MyVampPlugin::reset()
{
}

MyVampPlugin::InputDomain MyVampPlugin::getInputDomain() const
{
   return TimeDomain;
}

MyVampPlugin::OutputList MyVampPlugin::getOutputDescriptors() const
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

MyVampPlugin::FeatureSet MyVampPlugin::process(
   const float* const* inputBuffers, Vamp::RealTime timestamp)
{
   return {};
}

MyVampPlugin::FeatureSet MyVampPlugin::getRemainingFeatures()
{
   return {};
}

std::string MyVampPlugin::getIdentifier() const
{
   return "my_vamp_plugin";
}

std::string MyVampPlugin::getName() const
{
   return "My Audacity Vamp Plugin";
}

std::string MyVampPlugin::getDescription() const
{
   return "test plugin";
}

std::string MyVampPlugin::getMaker() const
{
   return "Matthieu Hodgkinson";
}

std::string MyVampPlugin::getCopyright() const
{
   return "GPL3";
}

int MyVampPlugin::getPluginVersion() const
{
   return 0;
}
