/*
* Audacity: A Digital Audio Editor
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <numeric>
#include <vector>

#include "au3-mixer/ExtrapolatingCrossfader.h"

namespace {
constexpr auto TEST_SAMPLE_RATE = 44100.0;
constexpr auto TEST_CROSSFADE_DURATION = 0.01;
//! Shorter than the cross-fade, as reads may be
constexpr auto TEST_BLOCK_SIZE = 256;
//! A few partials and no noise: what a low-order predictor predicts fully, as music may be
constexpr std::array TEST_PARTIAL_FREQUENCIES { 110.0, 220.5, 331.0, 440.7, 1250.0 };
//! Their sum is full-scale at most
constexpr auto TEST_PARTIAL_AMPLITUDE = 1.0 / TEST_PARTIAL_FREQUENCIES.size();
constexpr auto TEST_SIGNAL_DURATION = 1.0;
//! When gains change in the middle of the signal, after enough history to predict from
constexpr auto TEST_CHANGE_TIME = TEST_SIGNAL_DURATION / 2;
constexpr auto TEST_GAIN_BEFORE = 0.5;
constexpr auto TEST_GAIN_AFTER = 1.0;

const auto crossfadeLength = static_cast<size_t>(std::lround(TEST_CROSSFADE_DURATION * TEST_SAMPLE_RATE));
constexpr auto signalLength = static_cast<size_t>(TEST_SIGNAL_DURATION * TEST_SAMPLE_RATE);
constexpr auto changeSample = static_cast<size_t>(TEST_CHANGE_TIME * TEST_SAMPLE_RATE);

//! The largest difference between consecutive samples of the full-scale signal
const auto maxSignalStep = std::accumulate(TEST_PARTIAL_FREQUENCIES.begin(), TEST_PARTIAL_FREQUENCIES.end(), 0.0,
                                           [](double sum, double frequency) {
    return sum + 2 * std::numbers::pi * frequency / TEST_SAMPLE_RATE * TEST_PARTIAL_AMPLITUDE;
});

std::vector<float> partials()
{
    std::vector<float> samples(signalLength);
    for (size_t i = 0; i < samples.size(); ++i) {
        auto sample = 0.0;
        for (const auto frequency : TEST_PARTIAL_FREQUENCIES) {
            sample += TEST_PARTIAL_AMPLITUDE * std::sin(2 * std::numbers::pi * frequency * i / TEST_SAMPLE_RATE);
        }
        samples[i] = static_cast<float>(sample);
    }
    return samples;
}

//! Processes `samples` a block at a time, starting a cross-fade at the start of the blocks starting at `crossfadeStarts`
void process(ExtrapolatingCrossfader& crossfader, std::vector<float>& samples, const std::vector<size_t>& crossfadeStarts)
{
    for (size_t position = 0; position < samples.size(); position += TEST_BLOCK_SIZE) {
        if (std::find(crossfadeStarts.begin(), crossfadeStarts.end(), position) != crossfadeStarts.end()) {
            crossfader.StartCrossfade();
        }
        crossfader.Process(samples.data() + position, std::min<size_t>(TEST_BLOCK_SIZE, samples.size() - position));
    }
}

double maxStep(const std::vector<float>& samples)
{
    auto result = 0.0;
    for (size_t i = 1; i < samples.size(); ++i) {
        result = std::max(result, static_cast<double>(std::abs(samples[i] - samples[i - 1])));
    }
    return result;
}

double peak(const std::vector<float>& samples)
{
    auto result = 0.0;
    for (const auto sample : samples) {
        result = std::max(result, static_cast<double>(std::abs(sample)));
    }
    return result;
}

//! The first sample of the block in which the gains change
constexpr auto changeBlockStart = changeSample / TEST_BLOCK_SIZE * TEST_BLOCK_SIZE;
}

TEST(ExtrapolatingCrossfaderTests, WithoutCrossfadesTheSignalIsUnchanged)
{
    const auto input = partials();
    auto output = input;
    ExtrapolatingCrossfader crossfader { crossfadeLength };

    process(crossfader, output, {});

    EXPECT_EQ(output, input);
}

TEST(ExtrapolatingCrossfaderTests, GainJumpIsSmoothed)
{
    //! [GIVEN] a gain jump at the start of a block
    auto discontinuous = partials();
    for (size_t i = 0; i < discontinuous.size(); ++i) {
        discontinuous[i] *= static_cast<float>(i < changeBlockStart ? TEST_GAIN_BEFORE : TEST_GAIN_AFTER);
    }
    ASSERT_GT(maxStep(discontinuous), maxSignalStep);

    //! [WHEN] a cross-fade is started there
    auto output = discontinuous;
    ExtrapolatingCrossfader crossfader { crossfadeLength };
    process(crossfader, output, { changeBlockStart });

    //! [THEN] the output steps no more than the signal and a ramp of the gain jump do, and doesn't exceed its peak
    constexpr auto gainJump = TEST_GAIN_AFTER - TEST_GAIN_BEFORE;
    EXPECT_LE(maxStep(output), maxSignalStep + gainJump / crossfadeLength);
    EXPECT_LE(peak(output), peak(discontinuous));

    //! [THEN] after the cross-fade, the output is the signal
    EXPECT_TRUE(std::equal(output.begin() + changeBlockStart + crossfadeLength, output.end(),
                           discontinuous.begin() + changeBlockStart + crossfadeLength));
}

TEST(ExtrapolatingCrossfaderTests, CrossfadesStartedBeforeThePreviousOnesEndStayBounded)
{
    //! [GIVEN] a gain alternating at every block, as when a fade is dragged across the playhead
    auto signal = partials();
    std::vector<size_t> crossfadeStarts;
    for (size_t position = changeBlockStart; position < signal.size(); position += TEST_BLOCK_SIZE) {
        const auto gain = (position / TEST_BLOCK_SIZE) % 2 ? TEST_GAIN_BEFORE : TEST_GAIN_AFTER;
        for (size_t i = position; i < std::min<size_t>(position + TEST_BLOCK_SIZE, signal.size()); ++i) {
            signal[i] *= static_cast<float>(gain);
        }
        crossfadeStarts.push_back(position);
    }
    ASSERT_LT(TEST_BLOCK_SIZE, crossfadeLength);

    //! [WHEN] a cross-fade is started at every change, before the previous one has ended
    auto output = signal;
    ExtrapolatingCrossfader crossfader { crossfadeLength };
    process(crossfader, output, crossfadeStarts);

    //! [THEN] the output steps no more than the signal and a ramp of the gain jumps do, and doesn't exceed its peak
    constexpr auto gainJump = TEST_GAIN_AFTER - TEST_GAIN_BEFORE;
    EXPECT_LE(maxStep(output), maxSignalStep + gainJump / crossfadeLength);
    EXPECT_LE(peak(output), peak(signal));
}

TEST(ExtrapolatingCrossfaderTests, CrossfadeFromSilenceFadesIn)
{
    //! [GIVEN] nothing processed yet, i.e. silence before
    auto output = partials();

    //! [WHEN] a cross-fade is started at the start
    ExtrapolatingCrossfader crossfader { crossfadeLength };
    process(crossfader, output, { 0 });

    //! [THEN] the signal fades in
    for (size_t i = 0; i < crossfadeLength; ++i) {
        ASSERT_TRUE(std::isfinite(output[i]));
    }
    EXPECT_LE(maxStep(output), maxSignalStep);
}
