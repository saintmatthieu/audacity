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
#include "DSPFilters/Bessel.h"

#include <array>
#include <optional>

namespace ClipAnalysis
{
OnsetDetector::OnsetDetector(int fftSize, double fftRate, bool needsOutput)
    // Use a hann function for analysis and synthesis (if synthesis there is),
    // an overlap of 2 ...
    : SpectrumTransformer { needsOutput, eWinFuncHann, eWinFuncHann, fftSize, 2,
                            // ... and neither leading nor trailing window,
                            // thanks.
                            false, false }
    , mFftSize(fftSize)
    , mFftRate(fftRate)
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
   static std::ofstream ofs { "C:/Users/saint/Downloads/OnsetDetector.txt" };
   for (auto i = 0u; i < mStepSize; ++i)
      ofs << outBuffer[i] << std::endl;
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

std::vector<double> OnsetDetector::GetOnsetDetectionResults() const
{
   constexpr auto useHighPass = false;
   if (!useHighPass)
      return mOdfVals;

   constexpr int highpassOrder = 4;
   constexpr int numChannels = 1;
   Dsp::SimpleFilter<Dsp::Bessel::HighPass<highpassOrder>, numChannels> hp;
   // Remove low-frequency component like swelling.
   constexpr auto cutoff = 3;
   hp.setup(highpassOrder, mFftRate, cutoff);
   auto f = mOdfVals;
   // Duplicate the vector to avoid transients.
   f.insert(f.end(), f.begin(), f.end());
   auto data = f.data();
   hp.process(f.size(), &data);
   f.erase(f.begin(), f.begin() + f.size() / 2);
   return f;
}
} // namespace ClipAnalysis
