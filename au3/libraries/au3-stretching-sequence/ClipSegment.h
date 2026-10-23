/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  ClipSegment.h

  Matthieu Hodgkinson

**********************************************************************/
#pragma once

#include "AudioSegment.h"
#include "au3-utility/Observer.h"
#include "PlaybackDirection.h"
#include <atomic>
#include <memory>
#include <optional>

class ClipInterface;

/*!
 * Plays a clip, time-stretched and pitch-shifted.
 *
 * Stretching is stateful: it is a stream, which reading where the previous
 * read ended continues. Reading elsewhere restarts it there.
 *
 * It is important that objects of this class are instantiated and destroyed on
 * the same thread, due to the owned Observer::Subscription.
 */
class STRETCHING_SEQUENCE_API ClipSegment final : public AudioSegment
{
public:
    explicit ClipSegment(const ClipInterface&);
    ~ClipSegment() override;

    // AudioSegment
    void GetFloats(
        float* const* buffers, sampleCount start, size_t len, PlaybackDirection) override;
    sampleCount NumSamples() const override;
    size_t NChannels() const override;

private:
    //! What reading elsewhere than where the previous read ended restarts
    struct Stream;

    const ClipInterface& mClip;
    std::unique_ptr<Stream> mStream;
    //! Where the stream continues
    std::optional<sampleCount> mExpectedStart;
    PlaybackDirection mDirection = PlaybackDirection::forward;

    bool mPreserveFormants;
    int mCentShift;
    std::atomic<bool> mUpdateFormantPreservation = false;
    std::atomic<bool> mUpdateCentShift = false;
    Observer::Subscription mOnSemitoneShiftChangeSubscription;
    Observer::Subscription mOnFormantPreservationChangeSubscription;
};
