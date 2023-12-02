#pragma once

#include "BeatFinder.h"

namespace MIR
{
class MirAudioSource;

class QueenMaryBeatFinder : public BeatFinder
{
public:
   QueenMaryBeatFinder(const MirAudioSource& source);
   std::optional<BeatInfo> GetBeats() const override;
   std::vector<double> GetOnsetDetectionFunction() const override;
   double GetOnsetDetectionFunctionSampleRate() const override;

private:
   const std::optional<BeatInfo> mBeatInfo;
   const std::vector<double> mOnsetDetectionFunction;
   const double mOnsetDetectionFunctionSampleRate = 0.;
};
} // namespace MIR
