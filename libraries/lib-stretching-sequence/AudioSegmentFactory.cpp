/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  AudioSegmentFactory.cpp

  Matthieu Hodgkinson

**********************************************************************/

#include "AudioSegmentFactory.h"
#include "ClipInterface.h"
#include "ClipSegment.h"
#include "SilenceSegment.h"

#include <algorithm>

using ClipConstHolder = std::shared_ptr<const ClipInterface>;

AudioSegmentFactory::AudioSegmentFactory(
   int sampleRate, int numChannels, BPS projectTempo,
   const ClipConstHolders& clips)
    : mClips { clips }
    , mSampleRate { sampleRate }
    , mNumChannels { numChannels }
    , mBps { projectTempo }
{
}

std::vector<std::shared_ptr<AudioSegment>>
AudioSegmentFactory::CreateAudioSegmentSequence(
   double playbackStartTime, PlaybackDirection direction) const
{
   return direction == PlaybackDirection::forward ?
             CreateAudioSegmentSequenceForward(playbackStartTime) :
             CreateAudioSegmentSequenceBackward(playbackStartTime);
}

std::vector<std::shared_ptr<AudioSegment>>
AudioSegmentFactory::CreateAudioSegmentSequenceForward(double t0) const
{
   ClipConstHolders sortedClips { mClips };
   std::sort(
      sortedClips.begin(), sortedClips.end(),
      [](const ClipConstHolder& a, const ClipConstHolder& b) {
         return a->GetPlayStartTime() < b->GetPlayStartTime();
      });
   std::vector<std::shared_ptr<AudioSegment>> segments;
   for (const auto& clip : sortedClips)
   {
      if (clip->GetPlayStartTime(mBps) > t0)
      {
         const auto numSamples =
            sampleCount { (clip->GetPlayStartTime(mBps) - t0) * mSampleRate +
                          .5 };
         segments.push_back(
            std::make_shared<SilenceSegment>(mNumChannels, numSamples));
         t0 = clip->GetPlayStartTime() / mBps;
      }
      else if (clip->GetPlayEndTime(mBps) <= t0)
         continue;
      segments.push_back(std::make_shared<ClipSegment>(
         *clip, t0 - clip->GetPlayStartTime(mBps), PlaybackDirection::forward));
      t0 = clip->GetPlayEndTime(mBps);
   }
   return segments;
}

std::vector<std::shared_ptr<AudioSegment>>
AudioSegmentFactory::CreateAudioSegmentSequenceBackward(double t0) const
{
   ClipConstHolders sortedClips { mClips };
   std::sort(
      sortedClips.begin(), sortedClips.end(),
      [&](const ClipConstHolder& a, const ClipConstHolder& b) {
         return a->GetPlayEndTime() > b->GetPlayEndTime();
      });
   std::vector<std::shared_ptr<AudioSegment>> segments;
   for (const auto& clip : sortedClips)
   {
      if (clip->GetPlayEndTime(mBps) < t0)
      {
         const auto numSamples =
            sampleCount { (t0 - clip->GetPlayEndTime(mBps)) * mSampleRate +
                          .5 };
         segments.push_back(
            std::make_shared<SilenceSegment>(mNumChannels, numSamples));
         t0 = clip->GetPlayEndTime(mBps);
      }
      else if (clip->GetPlayStartTime(mBps) >= t0)
         continue;
      segments.push_back(std::make_shared<ClipSegment>(
         *clip, clip->GetPlayEndTime(mBps) - t0, PlaybackDirection::backward));
      t0 = clip->GetPlayStartTime(mBps);
   }
   return segments;
}
