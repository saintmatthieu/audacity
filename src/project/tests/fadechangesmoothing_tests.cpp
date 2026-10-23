/*
* Audacity: A Digital Audio Editor
*/

#include <gtest/gtest.h>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "au3wrap/internal/au3project.h"
#include "au3-project/Project.h"
#include "au3-clip-processing-sequence/ClipFades.h"
#include "au3-clip-processing-sequence/ClipSegment.h"
#include "au3-clip-processing-sequence/ClipProcessingSequence.h"
#include "au3-wave-track/WaveClip.h"
#include "au3-wave-track/WaveTrack.h"
#include "testing/testcontext.h"

namespace {
constexpr auto TEST_SAMPLE_RATE = 44100;
constexpr auto TEST_FREQUENCY = 440.0;
constexpr auto TEST_CLIP_DURATION = 1.0;
//! Changes of the fade-in don't affect the second half of the clip
constexpr auto TEST_FADE_IN = TEST_CLIP_DURATION / 2;
//! Smaller than the cross-fade, which then spans several reads
constexpr size_t TEST_BUFFER_SIZE = 128;
//! Between the output and what is expected, which are computed alike but separately
constexpr auto TEST_TOLERANCE = 1e-6;

//! The largest difference between consecutive samples of a full-scale sinusoid
constexpr auto maxSignalStep = 2 * std::numbers::pi * TEST_FREQUENCY / TEST_SAMPLE_RATE;
const auto crossfadeLength = static_cast<size_t>(std::lround(ClipSegment::fadeChangeCrossfadeDuration * TEST_SAMPLE_RATE));

float cosine(size_t i)
{
    return static_cast<float>(std::cos(2 * std::numbers::pi * TEST_FREQUENCY * i / TEST_SAMPLE_RATE));
}

//! The `i`th sample, with a linear fade-in of `fadeIn` seconds and no change during playback
float expectedSample(size_t i, double fadeIn)
{
    const auto t = static_cast<double>(i) / TEST_SAMPLE_RATE;
    return cosine(i) * static_cast<float>(std::min(t / fadeIn, 1.0));
}
}

namespace au::project {
class FadeChangeSmoothingTests : public ::testing::Test
{
protected:
    QTemporaryDir m_directory;
    au3::Au3ProjectAccessor m_accessor { au::testutils::makeTestContext() };
    std::shared_ptr<WaveTrack> m_track;
    std::shared_ptr<WaveClip> m_clip;

    ::AudacityProject& project()
    {
        return *reinterpret_cast<::AudacityProject*>(m_accessor.au3ProjectPtr());
    }

    void SetUp() override
    {
        const auto path = m_directory.filePath("smoothing.aup4");
        ASSERT_TRUE(QFile::copy(QString::fromUtf8(au_project_tests_DATA_ROOT) + "/data/empty.aup4", path));
        ASSERT_TRUE(m_accessor.load(path.toStdString(), false));

        //! A full-scale cosine, at its peak in the middle of the fade-in, where the fades are changed
        m_track = WaveTrackFactory::Get(project()).Create(floatSample, TEST_SAMPLE_RATE);
        m_clip = m_track->CreateClip(0.0, "clip");
        std::vector<float> samples(TEST_CLIP_DURATION * TEST_SAMPLE_RATE);
        for (size_t i = 0; i < samples.size(); ++i) {
            samples[i] = cosine(i);
        }
        auto data = reinterpret_cast<constSamplePtr>(samples.data());
        m_clip->Append(&data, floatSample, samples.size(), 1, floatSample);
        m_clip->Flush();
        m_track->InsertInterval(m_clip, true);

        ClipFades::Get(*m_clip).setFadeIn(TEST_FADE_IN);
    }

    void TearDown() override
    {
        m_accessor.close();
    }

    std::shared_ptr<ClipProcessingSequence> sequence()
    {
        return ClipProcessingSequence::Create(*m_track, m_track->GetClipInterfaces());
    }

    //! Reads from `from` to `to`, `TEST_BUFFER_SIZE` samples at a time, as during playback, appending to `samples`
    static void read(const PlayableSequence& sequence, size_t from, size_t to, std::vector<float>& samples)
    {
        for (auto position = from; position < to; position += TEST_BUFFER_SIZE) {
            std::vector<float> buffer(std::min(TEST_BUFFER_SIZE, to - position));
            const auto ptr = reinterpret_cast<samplePtr>(buffer.data());
            EXPECT_TRUE(sequence.DoGet(0, 1, &ptr, floatSample, position, buffer.size(), false));
            samples.insert(samples.end(), buffer.begin(), buffer.end());
        }
    }

    static void expectNoStepAbove(const std::vector<float>& samples, double maxStep)
    {
        for (size_t i = 1; i < samples.size(); ++i) {
            EXPECT_LE(std::abs(samples[i] - samples[i - 1]), maxStep) << "at sample " << i;
        }
    }

    //! From `from` on, the samples are as expected with a fade-in of `fadeIn`
    static void expectFadeIn(const std::vector<float>& samples, size_t from, double fadeIn)
    {
        for (auto i = from; i < samples.size(); ++i) {
            EXPECT_NEAR(samples[i], expectedSample(i, fadeIn), TEST_TOLERANCE) << "at sample " << i;
        }
    }
};

TEST_F(FadeChangeSmoothingTests, WithoutChangesTheFadesAreApplied)
{
    constexpr auto clipEnd = static_cast<size_t>(TEST_CLIP_DURATION * TEST_SAMPLE_RATE);
    std::vector<float> samples;
    read(*sequence(), 0, clipEnd, samples);

    expectFadeIn(samples, 0, TEST_FADE_IN);
}

TEST_F(FadeChangeSmoothingTests, FadeChangeDuringPlaybackIsCrossfaded)
{
    const auto playback = sequence();

    //! [GIVEN] reads up to the middle of the fade-in, at half gain
    constexpr auto changeTime = TEST_FADE_IN / 2;
    constexpr auto changeSample = static_cast<size_t>(changeTime * TEST_SAMPLE_RATE);
    std::vector<float> samples;
    read(*playback, 0, changeSample, samples);

    //! [WHEN] the fade-in is shortened to end there, i.e. the gain jumps to 1, and reading goes on
    constexpr auto shortenedFadeIn = changeTime;
    ClipFades::Get(*m_clip).setFadeIn(shortenedFadeIn);
    const auto end = changeSample + 2 * crossfadeLength;
    read(*playback, changeSample, end, samples);

    //! [THEN] unsmoothed, the output would jump more than the signal ever does
    ASSERT_GT(std::abs(expectedSample(changeSample, shortenedFadeIn) - expectedSample(changeSample - 1, TEST_FADE_IN)),
              maxSignalStep);

    //! [THEN] smoothed, it doesn't, the cross-fade only adding a ramp of the gain jump
    constexpr auto gainJump = 1.0 - changeTime / TEST_FADE_IN;
    expectNoStepAbove(samples, maxSignalStep + gainJump / crossfadeLength);

    //! [THEN] after the cross-fade, the output is as with the new fade-in
    expectFadeIn(samples, changeSample + crossfadeLength, shortenedFadeIn);
}

TEST_F(FadeChangeSmoothingTests, FadeChangeAwayFromTheReadPositionChangesNothing)
{
    const auto playback = sequence();

    //! [GIVEN] reads up to after the fade-in
    constexpr auto changeSample = static_cast<size_t>((TEST_FADE_IN + TEST_CLIP_DURATION) / 2 * TEST_SAMPLE_RATE);
    constexpr auto clipEnd = static_cast<size_t>(TEST_CLIP_DURATION * TEST_SAMPLE_RATE);
    std::vector<float> samples;
    read(*playback, 0, changeSample, samples);

    //! [WHEN] the fade-in is shortened, and reading goes on
    ClipFades::Get(*m_clip).setFadeIn(TEST_FADE_IN / 2);
    read(*playback, changeSample, clipEnd, samples);

    //! [THEN] the output is as without the change
    expectFadeIn(samples, 0, TEST_FADE_IN);
}

TEST_F(FadeChangeSmoothingTests, FadeChangeDuringACrossfadeIsCrossfadedFromIt)
{
    const auto playback = sequence();
    auto& fades = ClipFades::Get(*m_clip);

    //! [GIVEN] a cross-fade from half gain to full gain, halfway done
    constexpr auto changeTime = TEST_FADE_IN / 2;
    constexpr auto changeSample = static_cast<size_t>(changeTime * TEST_SAMPLE_RATE);
    std::vector<float> samples;
    read(*playback, 0, changeSample, samples);
    fades.setFadeIn(changeTime);
    const auto secondChangeSample = changeSample + crossfadeLength / 2;
    read(*playback, changeSample, secondChangeSample, samples);

    //! [WHEN] the fade-in is changed back, i.e. the gain jumps back to half
    fades.setFadeIn(TEST_FADE_IN);
    const auto end = secondChangeSample + 2 * crossfadeLength;
    read(*playback, secondChangeSample, end, samples);

    //! [THEN] the output doesn't jump either
    constexpr auto gainJump = 1.0 - changeTime / TEST_FADE_IN;
    expectNoStepAbove(samples, maxSignalStep + gainJump / crossfadeLength);

    //! [THEN] after the cross-fade, the output is as with the original fade-in
    expectFadeIn(samples, secondChangeSample + crossfadeLength, TEST_FADE_IN);
}
}
