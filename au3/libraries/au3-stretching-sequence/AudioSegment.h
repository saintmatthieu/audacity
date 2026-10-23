/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  AudioSegment.h

  Matthieu Hodgkinson

**********************************************************************/

#pragma once

#include "PlaybackDirection.h"
#include "au3-math/SampleCount.h"

// Linux requires explicit include for size_t
#ifdef __linux__
#include <cstddef>
#endif

/**
 * @brief The audio of a clip, as played: its samples are at the rate of the
 * sequence it is in, counted from its play start.
 */
class STRETCHING_SEQUENCE_API AudioSegment
{
public:
    virtual ~AudioSegment();

    /**
     * @brief Fills `buffers` with `len` samples, in reading order: forward,
     * those from `start` (inclusive) to `start + len`; backward, those from
     * `start` (exclusive) down to `start - len`.
     * @pre the samples are within the segment.
     * @param buffers Pointers to buffers, one for each channel.
     */
    virtual void GetFloats(
        float* const* buffers, sampleCount start, size_t len, PlaybackDirection) = 0;

    virtual sampleCount NumSamples() const = 0;

    /**
     * @brief The number of channels in the segment.
     */
    virtual size_t NChannels() const = 0;
};
