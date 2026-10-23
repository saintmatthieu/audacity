/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  ClipSegment.cpp

  Matthieu Hodgkinson

**********************************************************************/
#include "ClipSegment.h"
#include "ClipFadesProcessor.h"
#include "ClipInterface.h"
#include "ClipTimeAndPitchSource.h"
#include "LookaheadSmoother.h"
#include "au3-time-and-pitch/StaffPadTimeAndPitch.h"
#include <cmath>
#include <functional>

namespace {
TimeAndPitchInterface::Parameters
GetStretchingParameters(const ClipInterface& clip)
{
    TimeAndPitchInterface::Parameters params;
    params.timeRatio = clip.GetStretchRatio();
    params.pitchRatio = std::pow(2., clip.GetCentShift() / 1200.);
    params.preserveFormants
        =clip.GetPitchAndSpeedPreset() == PitchAndSpeedPreset::OptimizeForVoice;
    return params;
}
} // namespace

struct ClipSegment::Stream {
    //! @param durationToDiscard play time, from the clip's play start if forward, from its play end if backward
    Stream(const ClipInterface& clip, const ClipFades& fades, double durationToDiscard, PlaybackDirection direction)
        : source{clip, durationToDiscard, direction}
        , stretcher{clip.GetRate(), clip.NChannels(), source, GetStretchingParameters(clip)}
        , fadesProcessor{clip, fades, durationToDiscard, direction}
        , fadesSmoother{
            fadesProcessor, [this](float* const* buffers, size_t len) { stretcher.GetSamples(buffers, len); },
            clip.NChannels(), static_cast<size_t>(std::lround(fadeChangeCrossfadeDuration * clip.GetRate()))}
    {
    }

    ClipTimeAndPitchSource source;
    // Refers to `source`, constructed before it
    StaffPadTimeAndPitch stretcher;
    //! Fades are applied after stretching, smoothly when they change during playback
    ClipFadesProcessor fadesProcessor;
    LookaheadSmoother fadesSmoother;
};

ClipSegment::ClipSegment(const ClipInterface& clip)
    : mClip{clip}
    , mFades{ClipFades::Get(clip)}
    , mPreserveFormants{clip.GetPitchAndSpeedPreset()
                        == PitchAndSpeedPreset::OptimizeForVoice}
    , mCentShift{clip.GetCentShift()}
    , mOnSemitoneShiftChangeSubscription{clip.SubscribeToCentShiftChange(
                                             [this](int cents) {
        mCentShift = cents;
        mUpdateCentShift = true;
    })},
    mOnFormantPreservationChangeSubscription {
    clip.SubscribeToPitchAndSpeedPresetChange(
        [this](PitchAndSpeedPreset preset) {
        mPreserveFormants
            =preset == PitchAndSpeedPreset::OptimizeForVoice;
        mUpdateFormantPreservation = true;
    })
}
{
}

ClipSegment::~ClipSegment()
{
    mOnSemitoneShiftChangeSubscription.Reset();
    mOnFormantPreservationChangeSubscription.Reset();
}

void ClipSegment::GetFloats(
    float* const* buffers, sampleCount start, size_t len, PlaybackDirection direction)
{
    if (!mStream || mExpectedStart != start || mDirection != direction) {
        const auto playTime = start.as_double() / mClip.GetRate();
        const auto durationToDiscard = direction == PlaybackDirection::forward
                                       ? playTime
                                       : NumSamples().as_double() / mClip.GetRate() - playTime;
        mStream = std::make_unique<Stream>(mClip, mFades, durationToDiscard, direction);
        mDirection = direction;
    }
    const auto signedLen = static_cast<sampleCount::type>(len);
    mExpectedStart = direction == PlaybackDirection::forward ? start + signedLen : start - signedLen;

    // Check if formant preservation of pitch shift needs to be updated.
    // This approach is not immune to a race condition, but it is unlikely and
    // not critical, as it would only affect one playback pass, during which the
    // user could easily correct the mistake if needed. On the other hand, we
    // cannot trust that the observer subscriptions do not get called after
    // destruction of this object, so better not do anything too sophisticated
    // there.
    if (mUpdateFormantPreservation.exchange(false)) {
        mStream->stretcher.OnFormantPreservationChange(mPreserveFormants);
    }
    if (mUpdateCentShift.exchange(false)) {
        mStream->stretcher.OnCentShiftChange(mCentShift);
    }
    mStream->fadesSmoother.GetSamples(buffers, len);
}

sampleCount ClipSegment::NumSamples() const
{
    return sampleCount { mClip.GetVisibleSampleCount().as_double() * mClip.GetStretchRatio() + .5 };
}

size_t ClipSegment::NChannels() const
{
    return mClip.NChannels();
}
