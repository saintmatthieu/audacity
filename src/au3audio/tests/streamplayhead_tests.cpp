/*
 * Audacity: A Digital Audio Editor
 */
#include <gtest/gtest.h>

#include <vector>

#include "../internal/juce/streamplayhead.h"

namespace {
constexpr int64_t TEST_LOOP_START = 100;
constexpr int64_t TEST_LOOP_END = 200;
constexpr int64_t TEST_LOOP_LENGTH = TEST_LOOP_END - TEST_LOOP_START;
constexpr int64_t TEST_POSITION_IN_LOOP = TEST_LOOP_START + TEST_LOOP_LENGTH / 2;
constexpr int64_t TEST_POSITION_BEFORE_LOOP = TEST_LOOP_START / 2;
constexpr int64_t TEST_POSITION_AFTER_LOOP = TEST_LOOP_END + TEST_LOOP_LENGTH;
//! Wraps around the loop several times
constexpr int64_t TEST_BLOCK_SIZE = 5 * TEST_LOOP_LENGTH + TEST_LOOP_LENGTH / 3;
constexpr int64_t TEST_DEVICE_ADVANCE = TEST_LOOP_LENGTH / 4;
}

namespace au::au3audio {
class StreamPlayHeadTests : public ::testing::Test
{
protected:
    StreamPlayHead m_playHead;

    //! The timeline positions of `length` indices from `index`, segment by segment
    std::vector<int64_t> positions(int64_t index, int64_t length) const
    {
        std::vector<int64_t> positions;
        for (int64_t done = 0; done < length;) {
            const auto [position, segmentLength] = m_playHead.segment(index + done, length - done);
            EXPECT_GT(segmentLength, 0);
            for (int64_t i = 0; i < segmentLength; ++i) {
                positions.push_back(position + i);
            }
            done += segmentLength;
        }
        return positions;
    }

    //! The positions the play head should give, looping from `from`
    static std::vector<int64_t> looped(int64_t from, int64_t length)
    {
        std::vector<int64_t> positions;
        for (auto position = from; static_cast<int64_t>(positions.size()) < length; ++position) {
            if (position == TEST_LOOP_END) {
                position = TEST_LOOP_START;
            }
            positions.push_back(position);
        }
        return positions;
    }

    static std::vector<int64_t> linear(int64_t from, int64_t length)
    {
        std::vector<int64_t> positions;
        for (int64_t i = 0; i < length; ++i) {
            positions.push_back(from + i);
        }
        return positions;
    }
};

TEST_F(StreamPlayHeadTests, PlaysLinearlyWithoutLoop)
{
    m_playHead.start(TEST_POSITION_IN_LOOP, std::nullopt);

    EXPECT_EQ(positions(0, TEST_BLOCK_SIZE), linear(TEST_POSITION_IN_LOOP, TEST_BLOCK_SIZE));
    EXPECT_EQ(m_playHead.linearPosition(TEST_BLOCK_SIZE), TEST_POSITION_IN_LOOP + TEST_BLOCK_SIZE);
    EXPECT_FALSE(m_playHead.loop().has_value());
}

TEST_F(StreamPlayHeadTests, LoopsFromWithinTheLoop)
{
    m_playHead.start(TEST_POSITION_IN_LOOP, StreamPlayHead::Range { TEST_LOOP_START, TEST_LOOP_END });

    EXPECT_EQ(positions(0, TEST_BLOCK_SIZE), looped(TEST_POSITION_IN_LOOP, TEST_BLOCK_SIZE));
}

TEST_F(StreamPlayHeadTests, RollsIntoTheLoop)
{
    m_playHead.start(TEST_POSITION_BEFORE_LOOP, StreamPlayHead::Range { TEST_LOOP_START, TEST_LOOP_END });

    auto expected = linear(TEST_POSITION_BEFORE_LOOP, TEST_LOOP_START - TEST_POSITION_BEFORE_LOOP);
    const auto rest = looped(TEST_LOOP_START, TEST_BLOCK_SIZE - static_cast<int64_t>(expected.size()));
    expected.insert(expected.end(), rest.begin(), rest.end());
    EXPECT_EQ(positions(0, TEST_BLOCK_SIZE), expected);
}

TEST_F(StreamPlayHeadTests, StartsRightOfTheLoopAtItsStart)
{
    m_playHead.start(TEST_POSITION_AFTER_LOOP, StreamPlayHead::Range { TEST_LOOP_START, TEST_LOOP_END });

    EXPECT_EQ(m_playHead.position(), TEST_LOOP_START);
}

TEST_F(StreamPlayHeadTests, SeeksWhereTheDeviceIs)
{
    m_playHead.start(TEST_POSITION_BEFORE_LOOP, std::nullopt);
    m_playHead.advance(TEST_DEVICE_ADVANCE);

    m_playHead.setPosition(TEST_POSITION_AFTER_LOOP);

    EXPECT_EQ(m_playHead.position(), TEST_POSITION_AFTER_LOOP);
    EXPECT_EQ(positions(TEST_DEVICE_ADVANCE, TEST_BLOCK_SIZE), linear(TEST_POSITION_AFTER_LOOP, TEST_BLOCK_SIZE));
}

TEST_F(StreamPlayHeadTests, SeeksWithinTheLoopWhileLooping)
{
    m_playHead.start(TEST_POSITION_IN_LOOP, StreamPlayHead::Range { TEST_LOOP_START, TEST_LOOP_END });
    m_playHead.advance(TEST_DEVICE_ADVANCE);

    m_playHead.setPosition(TEST_POSITION_BEFORE_LOOP);
    EXPECT_EQ(m_playHead.position(), TEST_LOOP_START);

    m_playHead.setPosition(TEST_POSITION_AFTER_LOOP);
    EXPECT_EQ(m_playHead.position(), TEST_LOOP_END - 1);
}

TEST_F(StreamPlayHeadTests, RollsOnWhenSeekingWhileRollingIn)
{
    m_playHead.start(0, StreamPlayHead::Range { TEST_LOOP_START, TEST_LOOP_END });
    m_playHead.advance(TEST_DEVICE_ADVANCE);

    m_playHead.setPosition(TEST_POSITION_BEFORE_LOOP);

    EXPECT_EQ(m_playHead.position(), TEST_POSITION_BEFORE_LOOP);
}

TEST_F(StreamPlayHeadTests, PlaysOnWhenTheLoopIsTurnedOff)
{
    m_playHead.start(TEST_POSITION_IN_LOOP, StreamPlayHead::Range { TEST_LOOP_START, TEST_LOOP_END });
    m_playHead.advance(TEST_LOOP_LENGTH);

    m_playHead.setLoop(std::nullopt);

    EXPECT_EQ(positions(TEST_LOOP_LENGTH, TEST_BLOCK_SIZE), linear(TEST_POSITION_IN_LOOP, TEST_BLOCK_SIZE));
}

TEST_F(StreamPlayHeadTests, KeepsItsPositionWhenTheLoopEndMoves)
{
    constexpr int64_t newLoopEnd = TEST_LOOP_END + TEST_LOOP_LENGTH;
    m_playHead.start(TEST_POSITION_IN_LOOP, StreamPlayHead::Range { TEST_LOOP_START, TEST_LOOP_END });
    m_playHead.advance(TEST_DEVICE_ADVANCE);

    m_playHead.setLoop(StreamPlayHead::Range { TEST_LOOP_START, newLoopEnd });

    EXPECT_EQ(m_playHead.position(), TEST_POSITION_IN_LOOP + TEST_DEVICE_ADVANCE);
    EXPECT_EQ(m_playHead.segment(TEST_DEVICE_ADVANCE, TEST_BLOCK_SIZE).length, newLoopEnd - m_playHead.position());
}

TEST_F(StreamPlayHeadTests, JumpsIntoALoopMovedAway)
{
    m_playHead.start(TEST_POSITION_IN_LOOP, StreamPlayHead::Range { TEST_LOOP_START, TEST_LOOP_END });

    m_playHead.setLoop(StreamPlayHead::Range { TEST_POSITION_AFTER_LOOP, TEST_POSITION_AFTER_LOOP + TEST_LOOP_LENGTH });

    EXPECT_EQ(m_playHead.position(), TEST_POSITION_AFTER_LOOP);
}

TEST_F(StreamPlayHeadTests, ScheduleMapsAsThePlayHead)
{
    m_playHead.start(TEST_POSITION_IN_LOOP, StreamPlayHead::Range { TEST_LOOP_START, TEST_LOOP_END });
    const StreamPlayHead::Schedule schedule { m_playHead };

    const auto segment = schedule.Map(0, TEST_BLOCK_SIZE);

    EXPECT_EQ(segment.position, TEST_POSITION_IN_LOOP);
    EXPECT_EQ(static_cast<int64_t>(segment.len), TEST_LOOP_END - TEST_POSITION_IN_LOOP);
}
}
