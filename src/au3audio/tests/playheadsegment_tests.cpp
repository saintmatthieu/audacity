/*
 * Audacity: A Digital Audio Editor
 */
#include <gtest/gtest.h>

#include <vector>

#include "../internal/tracktion/playheadsegment.h"

namespace {
constexpr int64_t TEST_LOOP_START = 100;
constexpr int64_t TEST_LOOP_END = 200;
constexpr int64_t TEST_LOOP_LENGTH = TEST_LOOP_END - TEST_LOOP_START;
//! Wraps around the loop several times
constexpr int64_t TEST_BLOCK_SIZE = 5 * TEST_LOOP_LENGTH + TEST_LOOP_LENGTH / 3;
constexpr int64_t TEST_POSITION_IN_LOOP = TEST_LOOP_START + TEST_LOOP_LENGTH / 2;
constexpr int64_t TEST_POSITION_BEFORE_LOOP = TEST_LOOP_START / 2;
}

namespace au::au3audio {
class PlayHeadSegmentTests : public ::testing::Test
{
protected:
    tracktion::graph::PlayHead m_playHead;

    void SetUp() override
    {
        m_playHead.setReferenceSampleRange({ 0, 0 });
        m_playHead.play({ TEST_LOOP_START, TEST_LOOP_END }, true);
    }

    //! The positions of a block from the start of the reference samples, segment by segment
    std::vector<int64_t> segmentedPositions() const
    {
        const auto linearStart = m_playHead.referenceSampleRangeToSourceRangeUnlooped({ 0, TEST_BLOCK_SIZE }).getStart();
        std::vector<int64_t> positions;
        for (int64_t done = 0; done < TEST_BLOCK_SIZE;) {
            const auto [start, length] = playHeadSegment(m_playHead, linearStart + done, TEST_BLOCK_SIZE - done);
            EXPECT_GT(length, 0);
            for (int64_t i = 0; i < length; ++i) {
                positions.push_back(start + i);
            }
            done += length;
        }
        return positions;
    }

    //! The positions of a block, sample by sample, as the play head says
    std::vector<int64_t> samplePositions()
    {
        std::vector<int64_t> positions;
        for (int64_t i = 0; i < TEST_BLOCK_SIZE; ++i) {
            m_playHead.setReferenceSampleRange({ i, i + 1 });
            positions.push_back(m_playHead.getPosition());
        }
        return positions;
    }
};

TEST_F(PlayHeadSegmentTests, BlocksWrapAroundShortLoopsAsOftenAsThePlayHeadDoes)
{
    m_playHead.overridePosition(TEST_POSITION_IN_LOOP);
    const auto segmented = segmentedPositions();
    EXPECT_EQ(segmented, samplePositions());
}

TEST_F(PlayHeadSegmentTests, BlocksRollIntoShortLoopsAsThePlayHeadDoes)
{
    m_playHead.setRollInToLoop(TEST_POSITION_BEFORE_LOOP);
    const auto segmented = segmentedPositions();
    EXPECT_EQ(segmented, samplePositions());
}
}
