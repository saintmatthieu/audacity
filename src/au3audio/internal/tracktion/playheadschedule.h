/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include "au3-clip-processing/sequence-graph/PlaybackScheduleNode.h"

#include "tracktiongraphincludes.h"

namespace au::au4tracktion {
//! What of the timeline the device plays, as the play head maps it
/*!
 * Indices are reference samples, positions timeline samples. As the play head does, it plays linearly into a loop, and
 * then loops.
 */
class PlayHeadSchedule final : public PlaybackScheduleNode::ISchedule
{
public:
    explicit PlayHeadSchedule(const tracktion::graph::PlayHead& playHead);

    Segment Map(sampleCount index, size_t maxLen) const override;

private:
    const tracktion::graph::PlayHead& m_playHead;
};
}
