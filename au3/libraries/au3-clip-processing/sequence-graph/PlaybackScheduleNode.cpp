/*
 * Audacity: A Digital Audio Editor
 */
#include "PlaybackScheduleNode.h"

#include <algorithm>
#include <cassert>

PlaybackScheduleNode::PlaybackScheduleNode(std::shared_ptr<ISequenceProcessingNode> input, std::unique_ptr<ISchedule> schedule,
                                           size_t numChannels)
    : mInput{std::move(input)}
    , mSchedule{std::move(schedule)}
    , mBuffers(numChannels)
{
}

void PlaybackScheduleNode::GetSamples(float* const* buffers, sampleCount start, size_t len)
{
    size_t done = 0;
    while (done < len) {
        const auto segment = mSchedule->Map(start + done, len - done);
        assert(segment.len > 0);
        if (segment.len == 0) {
            for (size_t i = 0; i < mBuffers.size(); ++i) {
                std::fill_n(buffers[i] + done, len - done, 0.f);
            }
            return;
        }
        for (size_t i = 0; i < mBuffers.size(); ++i) {
            mBuffers[i] = buffers[i] + done;
        }
        mInput->GetSamples(mBuffers.data(), segment.position, segment.len);
        done += segment.len;
    }
}
