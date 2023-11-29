#include "GetVampFeatures.h"
#include "GetVampFeaturesFromPlugin.h"
#include "MirAudioSource.h"

#include <algorithm>
#include <cassert>
#include <vamp-hostsdk/PluginLoader.h>

namespace MIR
{
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
   return GetVampFeaturesFromPlugin<Vamp::Plugin::FeatureSet>(
      *plugin, source, config);
}
} // namespace MIR
