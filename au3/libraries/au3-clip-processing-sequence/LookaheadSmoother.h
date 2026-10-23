/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  LookaheadSmoother.h

**********************************************************************/
#pragma once

#include "au3-math/SampleCount.h"

#include <functional>
#include <vector>

//! Processes a stream of samples with parameters that may change while it is read, e.g. clip fades during playback
class CLIP_PROCESSING_SEQUENCE_API StreamProcessor
{
public:
    virtual ~StreamProcessor();

    //! Takes the latest parameters
    /*!
     * @return whether they changed
     */
    virtual bool UpdateParameters() = 0;

    //! Processes in place, with the current parameters
    /*!
     * @param position of the first sample, counted from the start of the stream
     * @pre the result only depends on the samples, their position and the parameters, as the same samples may be
     * processed again, with other parameters
     */
    virtual void Process(float* const* buffers, size_t len, sampleCount position) const = 0;
};

//! Processes a stream with a `StreamProcessor`, cross-fading when its parameters change, so that the output doesn't jump
/*!
 * It reads `lookahead` samples ahead, and keeps them, raw and as they are to be output. When the parameters change, it
 * processes them again, and cross-fades from what was to be output into that. Both being real, nothing is guessed.
 * Because what was to be output includes any cross-fade in progress, cross-fades chain without jumps.
 *
 * Changes take effect from the next sample output, over `lookahead` samples.
 */
class CLIP_PROCESSING_SEQUENCE_API LookaheadSmoother final
{
public:
    //! Provides the next `len` samples of the stream
    using Source = std::function<void (float* const* buffers, size_t len)>;

    LookaheadSmoother(StreamProcessor&, Source, size_t numChannels, size_t lookahead);

    void GetSamples(float* const* buffers, size_t len);

private:
    using ChannelBuffers = std::vector<std::vector<float> >;

    //! Pointers to `channelBuffers`, from `offset` on
    float* const* Pointers(ChannelBuffers& channelBuffers, size_t offset);

    StreamProcessor& mProcessor;
    const Source mSource;
    const size_t mLookahead;

    //! From the first sample read ahead: the raw samples ...
    ChannelBuffers mRaw;
    //! ... and as they are to be output
    ChannelBuffers mOutput;
    //! The samples read ahead, processed again after a change
    ChannelBuffers mReprocessed;
    std::vector<float*> mPointers;

    //! Position of the first sample read ahead
    sampleCount mPosition = 0;
    bool mReadAhead = false;
};
