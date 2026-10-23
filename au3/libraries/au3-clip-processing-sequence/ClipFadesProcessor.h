/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  ClipFadesProcessor.h

**********************************************************************/
#pragma once

#include "ClipFades.h"
#include "LookaheadSmoother.h"
#include "PlaybackDirection.h"

//! Applies the fades of a clip to the samples it plays, after stretching
class CLIP_PROCESSING_SEQUENCE_API ClipFadesProcessor final : public StreamProcessor
{
public:
    ClipFadesProcessor(const ClipInterface& clip, const ClipFades& fades, double durationToDiscard, PlaybackDirection);

    bool UpdateParameters() override;
    void Process(float* const* buffers, size_t len, sampleCount position) const override;

private:
    const ClipFades& mFades;
    const size_t mNumChannels;
    const double mRate;
    //! Play time, from the clip's play start if reading forward, from its play end if backward
    const double mDurationToDiscard;
    const PlaybackDirection mDirection;
    ClipFades::Snapshot mSnapshot;
};
