/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  AudioSegmentFactoryInterface.h

  Matthieu Hodgkinson

**********************************************************************/
#pragma once

#include <memory>
#include <vector>

class AudioSegment;
class ClipInterface;

//! A clip, and the segment playing it
struct ClipAudioSegment {
    std::shared_ptr<const ClipInterface> clip;
    std::shared_ptr<AudioSegment> segment;
};

class STRETCHING_SEQUENCE_API AudioSegmentFactoryInterface
{
public:
    virtual ~AudioSegmentFactoryInterface();

    //! Called on the main thread
    virtual std::vector<ClipAudioSegment> CreateClipAudioSegments() const = 0;
};
