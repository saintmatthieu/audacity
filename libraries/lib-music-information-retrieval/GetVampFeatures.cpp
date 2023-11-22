#include "GetVampFeatures.h"
#include "MirAudioSource.h"

#include <algorithm>
#include <cassert>
#include <vamp-hostsdk/PluginLoader.h>

namespace MIR
{
namespace
{
void concat(Vamp::Plugin::FeatureSet& a, const Vamp::Plugin::FeatureSet& b)
{
   for (const auto& [key, vectorToAppend] : b)
   {
      if (a.count(key) == 0)
         a[key] = {};
      auto& vector = a.at(key);
      vector.insert(vector.end(), vectorToAppend.begin(), vectorToAppend.end());
   }
}
} // namespace

Vamp::Plugin::FeatureSet GetVampFeatures(
   const std::string& pluginKey, const MirAudioSource& source,
   const VampPluginConfig& config)
{
   auto loader = Vamp::HostExt::PluginLoader::getInstance();
   const auto plugins = loader->listPlugins();
   const auto pluginIt = std::find_if(
      plugins.begin(), plugins.end(), [&pluginKey](const auto& plugin) {
         return plugin.find(pluginKey) != std::string::npos;
      });
   if (pluginIt == plugins.end())
      return {};
   const auto plugin = std::unique_ptr<Vamp::Plugin> { loader->loadPlugin(
      *pluginIt, source.GetSampleRate(),
      Vamp::HostExt::PluginLoader::ADAPT_INPUT_DOMAIN) };
   if (!plugin)
      return {};

   const auto blockSize = plugin->getPreferredBlockSize();
   const auto stepSize = plugin->getPreferredStepSize();
   constexpr auto numChannels = 1;
   const auto initialized =
      plugin->initialise(numChannels, stepSize, blockSize);
   assert(initialized);
   if (!initialized)
      return {};
   std::for_each(
      config.parameters.begin(), config.parameters.end(),
      [&plugin](const auto& parameter) {
         plugin->setParameter(parameter.first, parameter.second);
      });
   long start = 0;
   std::vector<float> buffer(blockSize);
   std::vector<float*> bufferPtr { buffer.data() };
   Vamp::Plugin::FeatureSet features;
   while (source.ReadFloats(buffer.data(), start, blockSize) == blockSize)
   {
      const auto timestamp = Vamp::RealTime::frame2RealTime(
         start, (int)(source.GetSampleRate() + 0.5));
      const auto newFeatures = plugin->process(bufferPtr.data(), timestamp);
      concat(features, newFeatures);
      start += stepSize;
   }
   concat(features, plugin->getRemainingFeatures());
   return features;
}
} // namespace MIR
