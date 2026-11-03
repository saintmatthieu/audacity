/*
* Audacity: A Digital Audio Editor
*/
#include "playheadsegment.h"

#include <algorithm>

std::pair<int64_t, int64_t> au::au3audio::playHeadSegment(const tracktion::graph::PlayHead& playHead, int64_t linearPosition,
                                                          int64_t maxLen)
{
    if (!playHead.isLooping()) {
        return { linearPosition, maxLen };
    }
    const auto loop = playHead.getLoopRange();
    // The play head stops rolling in when the device reaches the loop, which may be after this block
    if (playHead.isRollingIntoLoop() && linearPosition < loop.getStart()) {
        return { linearPosition, std::min(maxLen, loop.getStart() - linearPosition) };
    }
    const auto position = tracktion::graph::PlayHead::linearPositionToLoopPosition(linearPosition, loop);
    return { position, std::min(maxLen, loop.getEnd() - position) };
}
