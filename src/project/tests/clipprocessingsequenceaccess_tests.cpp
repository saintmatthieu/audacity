/*
* Audacity: A Digital Audio Editor
*/

#include <gtest/gtest.h>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
#include <random>
#include <vector>

#include "au3wrap/internal/au3project.h"
#include "au3-project/Project.h"
#include "au3-clip-processing-sequence/ClipProcessingSequence.h"
#include "au3-wave-track/WaveClip.h"
#include "au3-wave-track/WaveTrack.h"
#include "testing/testcontext.h"

namespace {
constexpr auto TEST_SAMPLE_RATE = 1000;
constexpr auto TEST_CLIP_DURATION = 1.0;
//! Two clips, with silence before, between and after them
constexpr auto TEST_FIRST_CLIP_START = 0.5;
constexpr auto TEST_SECOND_CLIP_START = 2.0;
constexpr auto TEST_TRACK_END = 3.5;
//! Shorter than the clips, and not dividing them, so that reads start and end anywhere
constexpr size_t TEST_BLOCK_SIZE = 77;

constexpr auto trackEndSample = static_cast<size_t>(TEST_TRACK_END * TEST_SAMPLE_RATE);
}

namespace au::project {
//! Without stretching, pitch shifting nor fades, reading the clip processing sequence is reading the track
class ClipProcessingSequenceAccessTests : public ::testing::Test
{
protected:
    QTemporaryDir m_directory;
    au3::Au3ProjectAccessor m_accessor { au::testutils::makeTestContext() };
    std::shared_ptr<WaveTrack> m_track;

    ::AudacityProject& project()
    {
        return *reinterpret_cast<::AudacityProject*>(m_accessor.au3ProjectPtr());
    }

    void SetUp() override
    {
        const auto path = m_directory.filePath("access.aup4");
        ASSERT_TRUE(QFile::copy(QString::fromUtf8(au_project_tests_DATA_ROOT) + "/data/empty.aup4", path));
        ASSERT_TRUE(m_accessor.load(path.toStdString(), false));

        //! Noise, so that any misplaced sample shows
        m_track = WaveTrackFactory::Get(project()).Create(floatSample, TEST_SAMPLE_RATE);
        std::mt19937 generator;
        std::uniform_real_distribution<float> noise(-1.f, 1.f);
        for (const auto clipStart : { TEST_FIRST_CLIP_START, TEST_SECOND_CLIP_START }) {
            const auto clip = m_track->CreateClip(clipStart, "clip");
            std::vector<float> samples(TEST_CLIP_DURATION * TEST_SAMPLE_RATE);
            std::generate(samples.begin(), samples.end(), [&] { return noise(generator); });
            auto data = reinterpret_cast<constSamplePtr>(samples.data());
            clip->Append(&data, floatSample, samples.size(), 1, floatSample);
            clip->Flush();
            m_track->InsertInterval(clip, true);
        }
    }

    void TearDown() override
    {
        m_accessor.close();
    }

    std::shared_ptr<ClipProcessingSequence> sequence()
    {
        return ClipProcessingSequence::Create(*m_track, m_track->GetClipInterfaces());
    }

    static std::vector<float> read(const WideSampleSequence& sequence, sampleCount start, size_t len, bool backwards)
    {
        std::vector<float> samples(len);
        float* const buffers[] { samples.data() };
        EXPECT_TRUE(sequence.GetFloats(0, 1, buffers, start, len, backwards));
        return samples;
    }

    //! Reads both, block by block, from `start` on, until `end`
    void expectSameReads(const WideSampleSequence& stretching, size_t start, size_t end, bool backwards)
    {
        for (auto position = start; backwards ? position > end : position < end;) {
            const auto len = std::min(TEST_BLOCK_SIZE, backwards ? position - end : end - position);
            ASSERT_EQ(read(stretching, position, len, backwards), read(*m_track, position, len, backwards))
                << "reading " << len << " samples from " << position << (backwards ? " backward" : " forward");
            position = backwards ? position - len : position + len;
        }
    }
};

TEST_F(ClipProcessingSequenceAccessTests, ReadingForwardIsReadingTheTrack)
{
    expectSameReads(*sequence(), 0, trackEndSample, false);
}

TEST_F(ClipProcessingSequenceAccessTests, ReadingBackwardIsReadingTheTrack)
{
    expectSameReads(*sequence(), trackEndSample, 0, true);
}

TEST_F(ClipProcessingSequenceAccessTests, ReadingAnywhereIsReadingTheTrack)
{
    const auto stretching = sequence();
    std::mt19937 generator;
    std::uniform_int_distribution<size_t> position(0, trackEndSample - TEST_BLOCK_SIZE);
    constexpr auto numReads = 100;
    for (auto i = 0; i < numReads; ++i) {
        const auto start = position(generator);
        // A few contiguous reads from there, in either direction
        const auto backwards = i % 2 == 1;
        constexpr auto numBlocks = 3;
        expectSameReads(*stretching, backwards ? start + TEST_BLOCK_SIZE : start,
                        backwards ? start + TEST_BLOCK_SIZE - std::min(start + TEST_BLOCK_SIZE, numBlocks * TEST_BLOCK_SIZE)
                        : std::min(start + numBlocks * TEST_BLOCK_SIZE, trackEndSample),
                        backwards);
    }
}
}
