/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  ClipSegment.cpp

  Matthieu Hodgkinson

**********************************************************************/
#include "ClipSegment.h"
#include "ClipInterface.h"
#include "SampleFormat.h"
#include "StaffPadTimeAndPitch.h"

#include <cassert>
#include <cmath>
#include <functional>

namespace
{
TimeAndPitchInterface::Parameters
GetStretchingParameters(const ClipInterface& clip)
{
   TimeAndPitchInterface::Parameters params;
   params.timeRatio = clip.GetStretchRatio();
   params.pitchRatio = std::pow(2., clip.GetCentShift() / 1200.);
   params.preserveFormants =
      clip.GetPitchAndSpeedPreset() == PitchAndSpeedPreset::OptimizeForVoice;
   return params;
}

sampleCount
GetTotalNumSamplesToProduce(const ClipInterface& clip, double durationToDiscard)
{
   return sampleCount { clip.GetVisibleSampleCount().as_double() *
                           clip.GetStretchRatio() -
                        durationToDiscard * clip.GetRate() + .5 };
}
} // namespace

ClipSegment::ClipSegment(
   ClipInterface& clip, double durationToDiscard, PlaybackDirection direction)
    : mTotalNumSamplesToProduce { GetTotalNumSamplesToProduce(
         clip, durationToDiscard) }
    , mSource { clip, durationToDiscard, direction }
    , mStretcher { std::make_unique<StaffPadTimeAndPitch>(
         clip.GetRate(), clip.GetWidth(), mSource,
         GetStretchingParameters(clip)) }
    , mOnSemitoneShiftChangeSubscription { clip.SubscribeToCentShiftChange(
         [this](int cents) { mStretcher->OnCentShiftChange(cents); }) }
    , mOnFormantPreservationChangeSubscription {
       clip.SubscribeToPitchAndSpeedPresetChange(
          [this](PitchAndSpeedPreset preset) {
             mStretcher->OnFormantPreservationChange(
                preset == PitchAndSpeedPreset::OptimizeForVoice);
          })
    }
{
}

size_t ClipSegment::GetFloats(float* const* buffers, size_t numSamples)
{
   const auto numSamplesToProduce = limitSampleBufferSize(
      numSamples, mTotalNumSamplesToProduce - mTotalNumSamplesProduced);
   mStretcher->GetSamples(buffers, numSamplesToProduce);
   mTotalNumSamplesProduced += numSamplesToProduce;
   return numSamplesToProduce;
}

bool ClipSegment::Empty() const
{
   return mTotalNumSamplesProduced == mTotalNumSamplesToProduce;
}

size_t ClipSegment::GetWidth() const
{
   return mSource.GetWidth();
}
