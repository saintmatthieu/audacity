/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  AudioSegmentFactory.h

  Matthieu Hodgkinson

**********************************************************************/

#pragma once

#include "AudioSegmentFactoryInterface.h"
#include "ClipInterface.h"
#include "au3-time-and-pitch/TimeAndPitchInterface.h"

#include <memory>

class ClipInterface;
using ClipConstHolders = std::vector<std::shared_ptr<const ClipInterface> >;

class STRETCHING_SEQUENCE_API AudioSegmentFactory final : public AudioSegmentFactoryInterface
{
public:
    explicit AudioSegmentFactory(ClipConstHolders clips);

    std::vector<ClipAudioSegment> CreateClipAudioSegments() const override;

private:
    const ClipConstHolders mClips;
};
