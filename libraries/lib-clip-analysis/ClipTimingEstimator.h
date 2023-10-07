#pragma once

#include "SpectrumTransformer.h"

class ClipInterface;

namespace ClipAnalysis
{
class ClipTimingEstimator : public SpectrumTransformer
{
public:
   ClipTimingEstimator(const ClipInterface& clip);
   void DoOutput(const float* outBuffer, size_t mStepSize) override;
   int Process();

private:
   const ClipInterface& mClip;
};
} // namespace ClipAnalysis
