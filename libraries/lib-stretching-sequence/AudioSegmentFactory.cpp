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
   double playbackStartTime, BPS tempo, PlaybackDirection direction) const
{
   return direction == PlaybackDirection::forward ?
             CreateAudioSegmentSequenceForward(playbackStartTime, tempo) :
             CreateAudioSegmentSequenceBackward(playbackStartTime, tempo);
}

std::vector<std::shared_ptr<AudioSegment>>
AudioSegmentFactory::CreateAudioSegmentSequenceForward(
   double t0, BPS tempo) const
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
      if (clip->GetPlayStartTime(tempo) > t0)
      {
         const auto numSamples =
            sampleCount { (clip->GetPlayStartTime(tempo) - t0) * mSampleRate +
                          .5 };
         segments.push_back(
            std::make_shared<SilenceSegment>(mNumChannels, numSamples));
         t0 = clip->GetPlayStartTime() / tempo;
      }
      else if (clip->GetPlayEndTime(tempo) <= t0)
         continue;
      segments.push_back(std::make_shared<ClipSegment>(
         *clip, tempo, t0 - clip->GetPlayStartTime(tempo),
         PlaybackDirection::forward));
      t0 = clip->GetPlayEndTime(tempo);
   }
   return segments;
}

std::vector<std::shared_ptr<AudioSegment>>
AudioSegmentFactory::CreateAudioSegmentSequenceBackward(
   double t0, BPS tempo) const
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
      if (clip->GetPlayEndTime(tempo) < t0)
      {
         const auto numSamples =
            sampleCount { (t0 - clip->GetPlayEndTime(tempo)) * mSampleRate +
                          .5 };
         segments.push_back(
            std::make_shared<SilenceSegment>(mNumChannels, numSamples));
         t0 = clip->GetPlayEndTime(tempo);
      }
      else if (clip->GetPlayStartTime(tempo) >= t0)
         continue;
      segments.push_back(std::make_shared<ClipSegment>(
         *clip, tempo, clip->GetPlayEndTime(tempo) - t0,
         PlaybackDirection::backward));
      t0 = clip->GetPlayStartTime(tempo);
   }
   return segments;
}
