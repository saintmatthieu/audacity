/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  ExtrapolatingCrossfader.h

**********************************************************************/
#pragma once

#include <cstddef>
#include <vector>

//! Smooths a discontinuity of a signal, by cross-fading from a continuation of the signal before it,
//! extrapolated by linear prediction, into the signal after it
/*!
 * Processes a single channel, in place, a block at a time.
 * Everything is allocated at construction: nothing is while processing.
 */
class MIXER_API ExtrapolatingCrossfader final
{
public:
    //! How many of the latest samples the continuation is extrapolated from
    static constexpr size_t defaultHistoryLength = 1024;
    //! Enough to extrapolate a few partials convincingly over a short cross-fade
    static constexpr size_t defaultPredictorOrder = 32;

    explicit ExtrapolatingCrossfader(
        size_t crossfadeLength, size_t historyLength = defaultHistoryLength, size_t predictorOrder = defaultPredictorOrder);

    //! The next sample processed is discontinuous with the previous ones
    /*!
     * During a cross-fade, the next sample is cross-faded into from what was output, i.e. from the partial cross-fade.
     */
    void StartCrossfade();

    void Process(float* samples, size_t len);

private:
    const size_t mCrossfadeLength;

    //! The latest output samples, in a circular buffer
    std::vector<float> mHistory;
    size_t mHistoryEnd = 0;

    //! Coefficients of the linear predictor, the first being 1
    std::vector<double> mPredictor;
    //! The latest samples of the extrapolated continuation, oldest first
    std::vector<double> mContinuation;
    //! Where in the cross-fade we are, if `mCrossfading`
    size_t mCrossfadePosition = 0;
    bool mCrossfading = false;

    //! Scratch buffers for the computation of the predictor
    std::vector<double> mForwardErrors;
    std::vector<double> mBackwardErrors;
};
