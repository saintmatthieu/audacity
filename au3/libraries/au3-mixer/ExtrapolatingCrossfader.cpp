/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  ExtrapolatingCrossfader.cpp

**********************************************************************/
#include "ExtrapolatingCrossfader.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace {
//! Below this fraction of the signal's energy, the prediction error is rounding noise, which higher orders would fit
constexpr auto negligiblePredictionError = 1e-9;

//! Computes `predictor` with Burg's method, so that `x[n] ~ -sum(predictor[k] * x[n - k], k = 1..order)`
/*!
 * @param forwardErrors the samples to predict, oldest first; overwritten
 * @param backwardErrors as large as `forwardErrors`; overwritten
 * @param predictor of size order + 1
 */
void ComputePredictor(std::vector<double>& forwardErrors, std::vector<double>& backwardErrors, std::vector<double>& predictor)
{
    auto& f = forwardErrors;
    auto& b = backwardErrors;
    std::copy(f.begin(), f.end(), b.begin());
    std::fill(predictor.begin(), predictor.end(), 0.0);
    predictor[0] = 1.0;

    const auto last = f.size() - 1;
    const auto order = predictor.size() - 1;
    auto denominator = 2 * std::inner_product(f.begin(), f.end(), f.begin(), 0.0) - f[0] * f[0] - b[last] * b[last];
    const auto minDenominator = std::max(denominator * negligiblePredictionError, std::numeric_limits<double>::min());

    for (size_t k = 0; k < order; ++k) {
        // E.g. silence, or a few pure partials, fully predicted by a lower order: what is computed so far will do.
        // Going on would fit rounding errors, and make the predictor unstable.
        if (!(denominator > minDenominator)) {
            return;
        }

        auto mu = 0.0;
        for (size_t n = 0; n < last - k; ++n) {
            mu += f[n + k + 1] * b[n];
        }
        mu *= -2 / denominator;
        // Can only happen with rounding errors; the predictor would be unstable
        if (!(std::abs(mu) < 1)) {
            return;
        }

        for (size_t n = 0; n <= (k + 1) / 2; ++n) {
            const auto a = predictor[n] + mu * predictor[k + 1 - n];
            predictor[k + 1 - n] += mu * predictor[n];
            predictor[n] = a;
        }

        for (size_t n = 0; n < last - k; ++n) {
            const auto forward = f[n + k + 1] + mu * b[n];
            b[n] += mu * f[n + k + 1];
            f[n + k + 1] = forward;
        }

        denominator = (1 - mu * mu) * denominator - f[k + 1] * f[k + 1] - b[last - k - 1] * b[last - k - 1];
    }
}
}

ExtrapolatingCrossfader::ExtrapolatingCrossfader(size_t crossfadeLength, size_t historyLength, size_t predictorOrder)
    : mCrossfadeLength{std::max<size_t>(crossfadeLength, 1)}
    , mHistory(historyLength)
    , mPredictor(predictorOrder + 1)
    , mContinuation(predictorOrder)
    , mForwardErrors(historyLength)
    , mBackwardErrors(historyLength)
{
}

void ExtrapolatingCrossfader::StartCrossfade()
{
    // Unroll the history, oldest first
    const auto historyStart = mHistory.begin() + mHistoryEnd;
    std::copy(historyStart, mHistory.end(), mForwardErrors.begin());
    std::copy(mHistory.begin(), historyStart, mForwardErrors.begin() + (mHistory.end() - historyStart));

    // The continuation starts from the latest samples ...
    std::copy(mForwardErrors.end() - mContinuation.size(), mForwardErrors.end(), mContinuation.begin());
    // ... before `mForwardErrors` is overwritten
    ComputePredictor(mForwardErrors, mBackwardErrors, mPredictor);

    mCrossfadePosition = 0;
    mCrossfading = true;
}

void ExtrapolatingCrossfader::Process(float* samples, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        if (mCrossfading) {
            auto extrapolated = 0.0;
            for (size_t k = 1; k < mPredictor.size(); ++k) {
                extrapolated -= mPredictor[k] * mContinuation[mContinuation.size() - k];
            }
            std::copy(mContinuation.begin() + 1, mContinuation.end(), mContinuation.begin());
            mContinuation.back() = extrapolated;

            const auto weight = static_cast<double>(mCrossfadePosition + 1) / mCrossfadeLength;
            samples[i] = static_cast<float>(std::lerp(extrapolated, static_cast<double>(samples[i]), weight));
            mCrossfading = ++mCrossfadePosition < mCrossfadeLength;
        }

        mHistory[mHistoryEnd] = samples[i];
        mHistoryEnd = (mHistoryEnd + 1) % mHistory.size();
    }
}
