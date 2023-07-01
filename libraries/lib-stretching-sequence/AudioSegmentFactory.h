/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  AudioSegmentFactory.h

  Matthieu Hodgkinson

**********************************************************************/

#pragma once

#include "AudioSegmentFactoryInterface.h"
#include "ClipInterface.h"

using ClipConstHolders = std::vector<std::shared_ptr<const ClipInterface>>;

class STRETCHING_SEQUENCE_API AudioSegmentFactory final :
    public AudioSegmentFactoryInterface
{
public:
   AudioSegmentFactory(
      int sampleRate, int numChannels, const ClipConstHolders& clips);

   std::vector<std::shared_ptr<AudioSegment>> CreateAudioSegmentSequence(
      double playbackStartTime, BPS tempo, PlaybackDirection) const override;

private:
   std::vector<std::shared_ptr<AudioSegment>>
   CreateAudioSegmentSequenceForward(double playbackStartTime, BPS tempo) const;

   std::vector<std::shared_ptr<AudioSegment>>
   CreateAudioSegmentSequenceBackward(
      double playbackStartTime, BPS tempo) const;

private:
   const ClipConstHolders mClips;
   const int mSampleRate;
   const int mNumChannels;
};
