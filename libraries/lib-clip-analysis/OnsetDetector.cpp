/*  SPDX-License-Identifier: GPL-2.0-or-later */
/**********************************************************************

  Audacity: A Digital Audio Editor

  OnsetDetector.cpp

  Matthieu Hodgkinson

  This is the onset-detection part of Adam Stark's BTrack implementation,
refactored for use in Audacity.

*******************************************************************/

#include "OnsetDetector.h"
#include "AudioSegmentSampleView.h"
#include "ClipInterface.h"

#include <array>
#include <optional>

namespace ClipAnalysis
{
OnsetDetector::OnsetDetector(int fftSize)
    // No output needed, use a hann function for analysis, a rectangular for
    // synthesis (that won't be used anyway), an FFT size of approximately 20ms
    // and a overlap of 2 for a hop size of about 10ms, as recommended in
    // Stark's paper. (Have to review this.)
    : SpectrumTransformer { false, eWinFuncHann, eWinFuncRectangular, fftSize,
                            2,
                            // We say no to the leading windows, since we're
                            // feeding the last fftSize/2 samples of the clip
                            // first. But we'll need the trailing windows. This
                            // should yield a complete cycle ... but crap ...
                            // the loop would have to be a multiple of the
                            // overlap ...
                            false, true }
    , mFftSize(fftSize)
{
   mPrevPhase.resize(mFftSize / 2);
   mPrevPhase2.resize(mFftSize / 2);
   mPrevMagSpec.resize(mFftSize / 2);
   std::fill(mPrevPhase.begin(), mPrevPhase.end(), 0.f);
   std::fill(mPrevPhase2.begin(), mPrevPhase2.end(), 0.f);
   std::fill(mPrevMagSpec.begin(), mPrevMagSpec.end(), 0.f);
}

void OnsetDetector::DoOutput(const float* outBuffer, size_t mStepSize)
{
   assert(false);
}

bool OnsetDetector::WindowProcessor(SpectrumTransformer& transformer)
{
   auto sum = 0.; // initialise sum to zero

   const Window& win = transformer.Latest();
   const auto& real = win.mRealFFTs;
   const auto& imag = win.mImagFFTs;

   // compute phase values from fft output and sum deviations
   for (auto i = 0u; i < real.size(); ++i)
   {
      // calculate phase value
      const auto phase = std::atan2(imag[i], real[i]);

      // calculate magnitude value
      const auto magSpec = std::sqrt(real[i] * real[i] + imag[i] * imag[i]);

      // phase deviation
      const auto phaseDeviation = phase - (2 * mPrevPhase[i]) + mPrevPhase2[i];

      // calculate magnitude difference (real part of Euclidean
      // distance between complex frames)
      const auto magnitudeDifference = magSpec - mPrevMagSpec[i];

      // if we have a positive change in magnitude, then include in
      // sum, otherwise ignore (half-wave rectification)
      if (magnitudeDifference > 0)
      {
         // calculate complex spectral difference for the current
         // spectral bin
         const auto csd = std::sqrt(
            magSpec * magSpec + mPrevMagSpec[i] * mPrevMagSpec[i] -
            2 * magSpec * mPrevMagSpec[i] * std::cos(phaseDeviation));

         // add to sum
         sum += csd;
      }

      // store values for next calculation
      mPrevPhase2[i] = mPrevPhase[i];
      mPrevPhase[i] = phase;
      mPrevMagSpec[i] = magSpec;
   }

   const auto avg = sum * 2 / mFftSize;

   mOdfVals.push_back(avg);

   return true;
}

const std::vector<double> OnsetDetector::GetOnsetDetectionResults() const
{
   return mOdfVals;
}
} // namespace ClipAnalysis
