/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  FormantShifter.cpp

  Matthieu Hodgkinson

**********************************************************************/
#include "FormantShifter.h"
#include "FormantShifterLoggerInterface.h"
#include <algorithm>
#include <fstream>

namespace
{
constexpr auto MapToPositiveHalfIndex(int index, int fullSize)
{
   const auto inputIsValid = fullSize > 0 && fullSize % 2 == 0;
   if (!inputIsValid)
      return 0;
   if (index >= 0)
      index = index % fullSize;
   else
      index = fullSize - (-index % fullSize);
   if (index > fullSize / 2)
      index = fullSize - index;
   return index;
}

static_assert(MapToPositiveHalfIndex(-7, 4) == 1);
static_assert(MapToPositiveHalfIndex(9, 4) == 1);

constexpr float GetLog2(float x)
{
   // https://stackoverflow.com/a/28730362
   // Precise up to two decimal places. If given unnormalized power, this
   // shouldn't be a problem.
   union
   {
      float val;
      int32_t x;
   } u = { x };
   auto log_2 = (float)(((u.x >> 23) & 255) - 128);
   u.x &= ~(255 << 23);
   u.x += 127 << 23;
   log_2 += ((-0.3358287811f) * u.val + 2.0f) * u.val - 0.65871759316667f;
   return log_2;
}

// `x` has length `fftSize/2+1`.
// Returns the last bin that wasn't zeroed.
size_t resampleFreqDomain(float* x, size_t fftSize, double factor)
{
   const auto size = fftSize / 2 + 1;
   const auto end = std::min(size, size_t(size * factor));
   std::vector<float> tmp(end);
   for (size_t i = 0; i < end; ++i)
   {
      const int int_pos = i / factor;
      const float frac_pos = 1.f * i / factor - int_pos;
      const auto k = MapToPositiveHalfIndex(int_pos, fftSize);
      const auto l = MapToPositiveHalfIndex(int_pos + 1, fftSize);
      tmp[i] = (1 - frac_pos) * x[k] + frac_pos * x[l];
   }
   std::copy(tmp.begin(), tmp.begin() + end, x);
   if (end < size)
      std::fill(x + end, x + size, 0.f);
   return end;
}
} // namespace

FormantShifter::FormantShifter(
   int sampleRate, double cutoffQuefrency,
   FormantShifterLoggerInterface& logger)
    : cutoffQuefrency { cutoffQuefrency }
    , mSampleRate { sampleRate }
    , mLogger { logger }
{
}

void FormantShifter::Reset(size_t fftSize)
{
   mFft = std::make_unique<staffpad::audio::FourierTransform>(fftSize);
   const auto numBins = fftSize / 2 + 1;
   mEnvelope.setSize(1, numBins);
   mCepstrum.setSize(1, fftSize);
   mEnvelopeReal.resize(numBins);
   mWeights.resize(numBins);
}

void FormantShifter::Reset()
{
   mFft.reset();
}

void FormantShifter::Process(
   const float* powSpec, std::complex<float>* spec, double factor)
{
   if (cutoffQuefrency == 0 || !mFft)
      return;

   const auto fftSize = mFft->getSize();
   const auto numBins = fftSize / 2 + 1;

   mLogger.Log(fftSize, "fftSize");

   // Take the log of the normalized magnitude. (This assumes that
   // the window averages to 1.)
   std::complex<float>* pEnv = mEnvelope.getPtr(0);
   const float normalizer = GetLog2(fftSize);
   std::transform(powSpec, powSpec + numBins, pEnv, [&](float power) {
      // Why -12 ?
      if (!power)
         return -12.f;
      return .5f * GetLog2(power) - normalizer;
   });

   // Get the cosine transform of the log magnitude, aka the cepstrum.
   mFft->inverseReal(mEnvelope, mCepstrum);
   auto pCepst = mCepstrum.getPtr(0);
   mLogger.Log(pCepst, fftSize, "cepstrum");

   // "Lifter" the cepstrum.
   const auto binCutoff = int(mSampleRate * cutoffQuefrency * factor);
   if (binCutoff < fftSize / 2)
      std::fill(pCepst + binCutoff + 1, pCepst + fftSize - binCutoff, 0.f);
   mLogger.Log(pCepst, fftSize, "cepstrumLiftered");

   // Get the envelope back.
   mFft->forwardReal(mCepstrum, mEnvelope);
   std::transform(
      pEnv, pEnv + numBins, mEnvelopeReal.begin(),
      [fftSize = fftSize](const std::complex<float>& env) {
         return std::exp2(env.real() / fftSize);
      });
   mLogger.Log(mEnvelopeReal.data(), numBins, "envelope");

   // Get the weights, which are the ratio of the desired envelope to the
   // current envelope (which has the effect of downsampling).
   std::transform(
      mEnvelopeReal.begin(), mEnvelopeReal.end(), mWeights.begin(),
      [](float env) { return std::isnormal(env) ? 1 / env : 0.f; });

   const auto lastNonZeroedBin =
      resampleFreqDomain(mEnvelopeReal.data(), fftSize, factor);

   mLogger.Log(mEnvelopeReal.data(), numBins, "envelopeResampled");
   std::transform(
      mEnvelopeReal.begin(), mEnvelopeReal.end(), mWeights.begin(),
      mWeights.begin(), std::multiplies<float> {});

   // Say the signal was downsampled to pitch it up. The factor is then less
   // than 1, and the resampler had to zero out the upper part of the envelope
   // bins. For these, rather than zeroing the spec too, it sounds better
   // to keep the original, even if no envelope correction is applied, else the
   // signal looses a bit of clarity. At such high frequencies, we probably
   // don't need a smooth frequency-domain transition and a jump is fine. (This
   // is visible in the spec, in case you're curious.)
   std::fill(mWeights.begin() + lastNonZeroedBin, mWeights.end(), 1.f);

   mLogger.Log(mWeights.data(), mWeights.size(), "weights");

   mLogger.Log(
      spec, numBins, "magnitude",
      [fftSize = fftSize](const std::complex<float>& spec) {
         return std::abs(spec) / fftSize;
      });

   // Now apply the weights.
   std::transform(
      spec, spec + numBins, mWeights.begin(), spec,
      std::multiplies<std::complex<float>>());

   mLogger.Log(
      spec, numBins, "weightedMagnitude",
      [fftSize = fftSize](const std::complex<float>& spec) {
         return std::abs(spec) / fftSize;
      });

   mLogger.ProcessFinished(spec, fftSize);
}
