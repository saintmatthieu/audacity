#pragma once

#include "MirTypes.h"

#include <memory>
#include <optional>

namespace MIR
{
class MirAudioSource;

class MUSIC_INFORMATION_RETRIEVAL_API BeatFinder
{
public:
   virtual ~BeatFinder() = default;
   virtual std::optional<BeatInfo> GetBeats() const = 0;
   virtual std::vector<double> GetOnsetDetectionFunction() const = 0;
   virtual double GetOnsetDetectionFunctionSampleRate() const = 0;

   static std::unique_ptr<BeatFinder> CreateInstance(
      const MirAudioSource& source, BeatTrackingAlgorithm algorithm);
};
} // namespace MIR
