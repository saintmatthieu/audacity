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
   int sampleRate, int numChannels, const ClipConstHolders& clips)
    : mClips { clips }
    , mSampleRate { sampleRate }
    , mNumChannels { numChannels }
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
      const auto clipStartTime = clip->GetPlayStartTime();
      const auto clipEndTime = clip->GetPlayEndTime();
      if (clipStartTime > t0)
      {
         const auto numSamples =
            sampleCount { (clipStartTime - t0) * mSampleRate + .5 };
         segments.push_back(std::make_shared<SilenceSegment>(
            mSampleRate, mNumChannels, t0, numSamples));
         t0 = clipStartTime;
      }
      else if (clipEndTime <= t0)
         continue;
      segments.push_back(std::make_shared<ClipSegment>(
         *clip, t0 - clipStartTime, PlaybackDirection::forward));
      t0 = clipEndTime;
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
      const auto clipStartTime = clip->GetPlayStartTime();
      const auto clipEndTime = clip->GetPlayEndTime();
      if (clipEndTime < t0)
      {
         const auto numSamples =
            sampleCount { (t0 - clipEndTime) * mSampleRate + .5 };
         segments.push_back(std::make_shared<SilenceSegment>(
            mSampleRate, mNumChannels, t0, numSamples));
         t0 = clipEndTime;
      }
      else if (clipStartTime >= t0)
         continue;
      segments.push_back(std::make_shared<ClipSegment>(
         *clip, clipEndTime - t0, PlaybackDirection::backward));
      t0 = clipStartTime;
   }
   return segments;
}
