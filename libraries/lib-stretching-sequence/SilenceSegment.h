/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  SilenceSegment.h

  Matthieu Hodgkinson

**********************************************************************/
#pragma once

#include "AudioSegment.h"

#include "SampleCount.h"

class STRETCHING_SEQUENCE_API SilenceSegment final : public AudioSegment
{
public:
   SilenceSegment(
      int sampleRate, size_t numChannels, double startTime,
      sampleCount numSamples);
   size_t GetFloats(std::vector<float*>& buffers, size_t numSamples) override;
   bool Empty() const override;
   size_t GetWidth() const override;

private:
   double GetPlayStartTime() const override;
   double GetPlayEndTime() const override;
   sampleCount TimeToSamples(double t) const override;

   const int mSampleRate;
   const size_t mNumChannels;
   const double mStartTime;
   const double mEndTime;
   sampleCount mNumRemainingSamples;
};
