/*
* Audacity: A Digital Audio Editor
*/
#include "playheadschedule.h"

#include "playheadsegment.h"

using namespace au::au3audio;

PlayHeadSchedule::PlayHeadSchedule(const tracktion::graph::PlayHead& playHead)
    : m_playHead{playHead}
{
}

PlaybackScheduleNode::ISchedule::Segment PlayHeadSchedule::Map(sampleCount index, size_t maxLen) const
{
    const auto linearPosition = m_playHead.referenceSamplePositionToTimelinePositionUnlooped(index.as_long_long());
    const auto [position, len] = playHeadSegment(m_playHead, linearPosition, static_cast<int64_t>(maxLen));
    return { position, static_cast<size_t>(len) };
}
