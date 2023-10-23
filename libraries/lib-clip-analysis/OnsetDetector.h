/*  SPDX-License-Identifier: GPL-2.0-or-later */
/**********************************************************************

  Audacity: A Digital Audio Editor

  OnsetDetector.h

  Matthieu Hodgkinson

*******************************************************************/

#pragma once

#include "SpectrumTransformer.h"

#include <optional>
#include <vector>

class ClipInterface;

namespace ClipAnalysis
{
class OnsetDetector : public SpectrumTransformer
{
public:
   OnsetDetector(int fftSize);
   bool WindowProcessor(SpectrumTransformer& transformer);
   const std::vector<double>& GetOnsetDetectionResults() const;

private:
   void DoOutput(const float* outBuffer, size_t mStepSize) override;

   const int mFftSize;
   std::vector<float> mPrevPhase;
   std::vector<float> mPrevPhase2;
   std::vector<float> mPrevMagSpec;
   std::vector<double> mOdfVals;
};
} // namespace ClipAnalysis
