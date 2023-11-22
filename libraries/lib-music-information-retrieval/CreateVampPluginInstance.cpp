#include "CreateVampPluginInstance.h"

#include <vamp-hostsdk/PluginLoader.h>

namespace MIR
{
std::unique_ptr<Vamp::Plugin>
GetVampPluginInstance(const std::string& pluginKey, double sampleRate)
{
   auto loader = Vamp::HostExt::PluginLoader::getInstance();
   const auto plugins = loader->listPlugins();
   // Find plugin `mvamp:marsyas_ibt`:
   const auto pluginIt =
      std::find_if(plugins.begin(), plugins.end(), [](const auto& plugin) {
         return plugin.find("qm-vamp-plugins:qm-barbeattracker") !=
                std::string::npos;
      });
   if (pluginIt == plugins.end())
      return {};
   return std::unique_ptr<Vamp::Plugin> { loader->loadPlugin(
      *pluginIt, sampleRate, Vamp::HostExt::PluginLoader::ADAPT_INPUT_DOMAIN) };
}
} // namespace MIR
