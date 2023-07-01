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

namespace
{
TimeAndPitchInterface::Parameters
GetStretchingParameters(const ClipInterface& clip, BPS projectTempo)
{
   TimeAndPitchInterface::Parameters params;
   params.timeRatio = clip.GetStretchRatio(projectTempo);
   return params;
}

sampleCount GetLastReadSample(
   const ClipInterface& clip, double durationToDiscard,
   PlaybackDirection direction, BPS projectTempo)
{
   if (direction == PlaybackDirection::forward)
      return sampleCount { clip.GetRate() * durationToDiscard /
                              clip.GetStretchRatio(projectTempo) +
                           .5 };
   else
      return clip.GetPlaySamplesCount() -
             sampleCount { clip.GetRate() * durationToDiscard /
                              clip.GetStretchRatio(projectTempo) +
                           .5 };
}

sampleCount GetTotalNumSamplesToProduce(
   const ClipInterface& clip, double durationToDiscard, BPS projectTempo)
{
   return sampleCount { (clip.GetPlaySamplesCount().as_double() -
                         durationToDiscard * clip.GetRate()) *
                           clip.GetStretchRatio(projectTempo) +
                        .5 };
}
} // namespace

ClipSegment::ClipSegment(
   const ClipInterface& clip, BPS projectTempo, double durationToDiscard,
   PlaybackDirection direction)
    : mClip { clip }
    , mBps { projectTempo }
    , mLastReadSample { GetLastReadSample(
         clip, durationToDiscard, direction, mBps) }
    , mTotalNumSamplesToProduce { GetTotalNumSamplesToProduce(
         clip, durationToDiscard, mBps) }
    , mPlaybackDirection { direction }
    , mStretcher { std::make_unique<StaffPadTimeAndPitch>(
         clip.GetWidth(), *this, GetStretchingParameters(clip, projectTempo)) }
{
}

size_t ClipSegment::GetFloats(std::vector<float*>& buffers, size_t numSamples)
{
   assert(buffers.size() == mClip.GetWidth());
   const auto numSamplesToProduce = limitSampleBufferSize(
      numSamples, mTotalNumSamplesToProduce - mTotalNumSamplesProduced);
   mStretcher->GetSamples(buffers.data(), numSamplesToProduce);
   mTotalNumSamplesProduced += numSamplesToProduce;
   return numSamplesToProduce;
}

bool ClipSegment::Empty() const
{
   return mTotalNumSamplesProduced == mTotalNumSamplesToProduce;
}

void ClipSegment::Pull(float* const* buffers, size_t samplesPerChannel)
{
   const auto forward = mPlaybackDirection == PlaybackDirection::forward;
   const auto remainingSamplesInClip =
      forward ? mClip.GetPlaySamplesCount() - mLastReadSample : mLastReadSample;
   const auto numSamplesToRead =
      std::min(sampleCount { samplesPerChannel }, remainingSamplesInClip)
         .as_size_t();
   if (numSamplesToRead > 0u)
   {
      constexpr auto mayThrow = false;
      const auto start =
         forward ? mLastReadSample : mLastReadSample - numSamplesToRead;
      const auto offset = forward ? 0u : samplesPerChannel - numSamplesToRead;
      const auto nChannels = mClip.GetWidth();
      ChannelSampleViews newViews;
      for (auto i = 0u; i < nChannels; ++i)
      {
         auto channelView = mClip.GetSampleView(i, start, numSamplesToRead);
         channelView.Copy(buffers[i] + offset, samplesPerChannel);
         newViews.push_back(std::move(channelView));
         if (!forward)
            ReverseSamples(
               reinterpret_cast<samplePtr>(buffers[i] + offset), floatSample, 0,
               numSamplesToRead);
      }
      mChannelSampleViews = std::move(newViews);
      mLastReadSample += forward ? numSamplesToRead : -numSamplesToRead;
   }
   if (numSamplesToRead < samplesPerChannel)
   {
      const auto begin = forward ? numSamplesToRead : 0;
      const auto end =
         forward ? samplesPerChannel : samplesPerChannel - numSamplesToRead;
      for (auto i = 0u; i < mClip.GetWidth(); ++i)
         std::fill(buffers[i] + begin, buffers[i] + end, 0.f);
   }
}
