/*
* Audacity: A Digital Audio Editor
*/
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "au3-clip-processing-sequence/LookaheadSmoother.h"

namespace {
constexpr size_t TEST_LOOKAHEAD = 8;
constexpr auto TEST_GAIN = 0.5;
constexpr auto TEST_OTHER_GAIN = 2.0;
constexpr auto TEST_THIRD_GAIN = -1.0;
constexpr size_t TEST_SIGNAL_LENGTH = 10 * TEST_LOOKAHEAD;
//! The smoother computes in single precision, the expectations in double precision
constexpr auto TEST_RELATIVE_TOLERANCE = 1e-6;

//! Multiplies by a gain, which can be changed at any time, taking effect at the next read
class GainProcessor final : public StreamProcessor
{
public:
    double nextGain = TEST_GAIN;

    bool UpdateParameters() override
    {
        const auto changed = nextGain != mGain;
        mGain = nextGain;
        return changed;
    }

    void Process(float* const* buffers, size_t len, sampleCount) const override
    {
        for (size_t i = 0; i < len; ++i) {
            buffers[0][i] *= static_cast<float>(mGain);
        }
    }

private:
    double mGain = TEST_GAIN;
};

class LookaheadSmootherTests : public ::testing::Test
{
protected:
    GainProcessor m_processor;
    //! The source counts from 0, so that the output tells which samples were read
    size_t m_sourcePosition = 0;
    LookaheadSmoother m_smoother { m_processor, [this](float* const* buffers, size_t len) {
            for (size_t i = 0; i < len; ++i) {
                buffers[0][i] = static_cast<float>(m_sourcePosition++);
            }
        }, 1, TEST_LOOKAHEAD };
    std::vector<float> m_output;

    void read(size_t len)
    {
        std::vector<float> buffer(len);
        float* const buffers[] { buffer.data() };
        m_smoother.GetSamples(buffers, len);
        m_output.insert(m_output.end(), buffer.begin(), buffer.end());
    }

    //! Reads blocks of `blockSize` until the output has `length` samples
    void readUpTo(size_t length, size_t blockSize)
    {
        while (m_output.size() < length) {
            read(std::min(blockSize, length - m_output.size()));
        }
    }

    //! The weight of the new gain at the `i`th sample of a cross-fade
    static double weight(size_t i)
    {
        return static_cast<double>(i + 1) / TEST_LOOKAHEAD;
    }

    void expectOutput(size_t n, double gain) const
    {
        ASSERT_NEAR(m_output[n], n * gain, std::abs(n * gain) * TEST_RELATIVE_TOLERANCE) << "at sample " << n;
    }
};
}

TEST_F(LookaheadSmootherTests, WithoutChangesTheStreamIsProcessed)
{
    // Blocks shorter and longer than the lookahead
    for (const auto blockSize : { size_t { 1 }, TEST_LOOKAHEAD - 1, TEST_LOOKAHEAD, 3 * TEST_LOOKAHEAD }) {
        readUpTo(m_output.size() + TEST_SIGNAL_LENGTH, blockSize);
    }

    for (size_t n = 0; n < m_output.size(); ++n) {
        expectOutput(n, TEST_GAIN);
    }
}

TEST_F(LookaheadSmootherTests, ChangeIsCrossfadedOverTheLookahead)
{
    //! [GIVEN] some output
    constexpr auto changeSample = TEST_SIGNAL_LENGTH / 2;
    constexpr auto blockSize = TEST_LOOKAHEAD / 2;
    readUpTo(changeSample, blockSize);

    //! [WHEN] the gain changes, and reading goes on
    m_processor.nextGain = TEST_OTHER_GAIN;
    readUpTo(TEST_SIGNAL_LENGTH, blockSize);

    //! [THEN] from the next sample on, the gain is cross-faded into the new one, over the lookahead
    for (size_t n = 0; n < m_output.size(); ++n) {
        const auto gain = n < changeSample ? TEST_GAIN
                          : n < changeSample + TEST_LOOKAHEAD ? std::lerp(TEST_GAIN, TEST_OTHER_GAIN, weight(n - changeSample))
                          : TEST_OTHER_GAIN;
        expectOutput(n, gain);
    }
}

TEST_F(LookaheadSmootherTests, ChangeDuringACrossfadeIsCrossfadedFromIt)
{
    //! [GIVEN] a cross-fade halfway done
    constexpr auto changeSample = TEST_SIGNAL_LENGTH / 2;
    constexpr auto secondChangeSample = changeSample + TEST_LOOKAHEAD / 2;
    constexpr auto blockSize = TEST_LOOKAHEAD / 4;
    readUpTo(changeSample, blockSize);
    m_processor.nextGain = TEST_OTHER_GAIN;
    readUpTo(secondChangeSample, blockSize);

    //! [WHEN] the gain changes again, and reading goes on
    m_processor.nextGain = TEST_THIRD_GAIN;
    readUpTo(TEST_SIGNAL_LENGTH, blockSize);

    //! [THEN] it is cross-faded from what the first cross-fade was to be into the latest gain
    for (auto n = secondChangeSample; n < m_output.size(); ++n) {
        const auto firstCrossfadeGain = n < changeSample + TEST_LOOKAHEAD
                                        ? std::lerp(TEST_GAIN, TEST_OTHER_GAIN, weight(n - changeSample))
                                        : TEST_OTHER_GAIN;
        const auto gain = n < secondChangeSample + TEST_LOOKAHEAD
                          ? std::lerp(firstCrossfadeGain, TEST_THIRD_GAIN, weight(n - secondChangeSample))
                          : TEST_THIRD_GAIN;
        expectOutput(n, gain);
    }
}
