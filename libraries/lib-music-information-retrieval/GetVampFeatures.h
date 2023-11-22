#pragma once

#include <vamp-hostsdk/Plugin.h>

#include <map>
#include <memory>
#include <optional>
#include <string>

namespace MIR
{
class MirAudioSource;

struct VampPluginConfig
{
   std::optional<double> frameSize;
   std::optional<double> hopSize;
   std::map<std::string, float> parameters;
};

Vamp::Plugin::FeatureSet GetVampFeatures(
   const std::string& pluginKey, const MirAudioSource& source,
   VampPluginConfig config = {});
} // namespace MIR
