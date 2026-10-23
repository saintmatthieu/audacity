/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  ClipFadesProcessor.cpp

**********************************************************************/
#include "ClipFadesProcessor.h"

ClipFadesProcessor::ClipFadesProcessor(
    const ClipInterface& clip, const ClipFades& fades, double durationToDiscard, PlaybackDirection direction)
    : mFades{fades}
    , mNumChannels{clip.NChannels()}
    , mRate{clip.GetRate()}
    , mDurationToDiscard{durationToDiscard}
    , mDirection{direction}
    , mSnapshot{mFades.snapshot()}
{
}

bool ClipFadesProcessor::UpdateParameters()
{
    const auto latest = mFades.snapshot();
    const auto changed = latest != mSnapshot;
    mSnapshot = latest;
    return changed;
}

void ClipFadesProcessor::Process(float* const* buffers, size_t len, sampleCount position) const
{
    for (size_t i = 0; i < len; ++i) {
        // Time of the sample from the clip's play start; backward, the first is the one before the start position
        const auto offset = (position + static_cast<sampleCount::type>(i)).as_double() / mRate;
        const auto t = mDirection == PlaybackDirection::forward
                       ? mDurationToDiscard + offset
                       : mSnapshot.playDuration - mDurationToDiscard - offset - 1 / mRate;
        const auto gain = mSnapshot.gainAt(t);
        if (gain == 1.0) {
            continue;
        }
        for (size_t c = 0; c < mNumChannels; ++c) {
            buffers[c][i] *= static_cast<float>(gain);
        }
    }
}
