/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "au3-mixer/ISequenceProcessingNode.h"

#include <memory>
#include <vector>

/**
 * @brief Its input as played: read by index in a stream, e.g. the samples of an audio device, where a schedule says
 * what of the input is played
 *
 * @details The index of a sample is when it is played, its position in the input what is played then. Where the stream
 * loops, e.g., a read is split into segments, each read from where the schedule says.
 */
class PlaybackScheduleNode final : public ISequenceProcessingNode
{
public:
    //! Says what of the input the stream plays
    class ISchedule
    {
    public:
        virtual ~ISchedule() = default;

        struct Segment {
            //! Where in the input
            sampleCount position;
            size_t len = 0;
        };

        //! Where in the input the stream plays from `index` on, and for how many samples contiguously, at most `maxLen`
        //! and at least one
        virtual Segment Map(sampleCount index, size_t maxLen) const = 0;
    };

    PlaybackScheduleNode(std::shared_ptr<ISequenceProcessingNode> input, std::unique_ptr<ISchedule> schedule, size_t numChannels);

    //! The samples of the stream from `start`
    void GetSamples(float* const* buffers, sampleCount start, size_t len) override;

private:
    const std::shared_ptr<ISequenceProcessingNode> mInput;
    const std::unique_ptr<ISchedule> mSchedule;
    std::vector<float*> mBuffers;
};
