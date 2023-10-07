/*  SPDX-License-Identifier: GPL-2.0-or-later */
/**********************************************************************

  Audacity: A Digital Audio Editor

  ClipOnsetDetector.h

  Matthieu Hodgkinson

*******************************************************************/

#pragma once

#include "SpectrumTransformer.h"

#include <optional>
#include <vector>

class ClipInterface;

namespace ClipAnalysis
{
class ClipOnsetDetector : public SpectrumTransformer
{
public:
   ClipOnsetDetector(const ClipInterface& clip);
   bool ClipWindowProcessor(SpectrumTransformer& transformer);
   const std::vector<double> GetOnsetDetectionResults() const;

private:
   void DoOutput(const float* outBuffer, size_t mStepSize) override;

   const ClipInterface& mClip;
   std::vector<float> mPrevPhase;
   std::vector<float> mPrevPhase2;
   std::vector<float> mPrevMagSpec;
   std::vector<double> mOdfVals;
};
} // namespace ClipAnalysis
