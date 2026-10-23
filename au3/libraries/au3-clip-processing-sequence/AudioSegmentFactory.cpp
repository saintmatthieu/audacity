/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  AudioSegmentFactory.cpp

  Matthieu Hodgkinson

**********************************************************************/

#include "AudioSegmentFactory.h"
#include "ClipInterface.h"
#include "ClipSegment.h"

AudioSegmentFactory::AudioSegmentFactory(ClipConstHolders clips)
    : mClips{std::move(clips)}
{
}

std::vector<ClipAudioSegment> AudioSegmentFactory::CreateClipAudioSegments() const
{
    std::vector<ClipAudioSegment> segments;
    segments.reserve(mClips.size());
    for (const auto& clip : mClips) {
        segments.push_back({ clip, std::make_shared<ClipSegment>(*clip) });
    }
    return segments;
}
