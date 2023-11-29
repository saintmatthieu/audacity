#pragma once

#include "MirTypes.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vamp-hostsdk/Plugin.h>

namespace MIR
{
class MirAudioSource;

Vamp::Plugin::FeatureSet GetVampFeatures(
   const std::string& pluginKey, const MirAudioSource& source,
   const VampPluginConfig& config = {});
} // namespace MIR
