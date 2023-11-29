#pragma once

#include "MirAudioSource.h"
#include "MirTypes.h"

#include <algorithm>

namespace MIR
{
namespace detail
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
} // namespace detail

template <typename FeatureSetType, typename VampPluginType>
FeatureSetType GetVampFeaturesFromPlugin(
   VampPluginType& plugin, const MirAudioSource& source,
   const VampPluginConfig& config)
{
   const auto blockSize =
      config.blockSize.value_or(plugin.getPreferredBlockSize());
   const auto stepSize =
      config.stepSize.value_or(plugin.getPreferredStepSize());
   constexpr auto numChannels = 1;
   if (!plugin.initialise(numChannels, stepSize, blockSize))
      return {};

   std::for_each(
      config.parameters.begin(), config.parameters.end(),
      [&plugin](const auto& parameter) {
         plugin.setParameter(parameter.first, parameter.second);
      });
   long start = 0;
   std::vector<float> buffer(blockSize);
   std::vector<float*> bufferPtr { buffer.data() };
   FeatureSetType features;
   while (source.ReadFloats(buffer.data(), start, blockSize) == blockSize)
   {
      const auto timestamp = Vamp::RealTime::frame2RealTime(
         start, (int)(source.GetSampleRate() + 0.5));
      const auto newFeatures = plugin.process(bufferPtr.data(), timestamp);
      detail::concat(features, newFeatures);
      start += stepSize;
   }
   detail::concat(features, plugin.getRemainingFeatures());
   return features;
}
} // namespace MIR
