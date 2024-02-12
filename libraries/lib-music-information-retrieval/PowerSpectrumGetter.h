/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  PowerSpectrumGetter.h

  Matthieu Hodgkinson

**********************************************************************/
#pragma once

#include "MirTypes.h"
#include <vector>

struct PFFFT_Setup;

namespace MIR
{
/*!
 * @brief Much faster that FFT.h's `PowerSpectrum`, at least in Short-Time
 * Fourier Transform-like situations, where many power spectra of the same size
 * are needed. Currently only power spectrum, but may be generalized to other
 * uses.
 */
class PowerSpectrumGetter
{
public:
   explicit PowerSpectrumGetter(int fftSize);
   ~PowerSpectrumGetter();

   /*!
    * @brief Computes the power spectrum of `buffer` into `output`.
    * @param buffer Input samples of size `fftSize`. Also gets used as
    * placeholder and gets overwritten, so copy your data elsewhere if you need
    * it again afterwards.
    * @param output `fftSize / 2 + 1` samples.
    */
   void operator()(PffftFloatVector& buffer, PffftFloatVector& output);

private:
   const int mFftSize;
   PFFFT_Setup* mSetup;
   PffftFloatVector mWork;
};
} // namespace MIR
