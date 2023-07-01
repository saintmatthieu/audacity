/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  ClipInterface.h

  Matthieu Hodgkinson

**********************************************************************/
#pragma once

#include "AudioSegmentSampleView.h"
#include "Beat.h"
#include "SampleCount.h"
#include "SampleFormat.h"

class STRETCHING_SEQUENCE_API ClipInterface
{
public:
   virtual ~ClipInterface();

   virtual AudioSegmentSampleView
   GetSampleView(size_t iChannel, sampleCount start, size_t length) const = 0;

   virtual sampleCount GetPlaySamplesCount() const = 0;

   virtual size_t GetWidth() const = 0;

   virtual int GetRate() const = 0;

   virtual Beat GetPlayStartTime() const = 0;
   virtual double GetPlayStartTime(BPS) const = 0;

   virtual Beat GetPlayEndTime() const = 0;
   virtual double GetPlayEndTime(BPS) const = 0;

   virtual double GetStretchRatio(BPS) const = 0;
};

using ClipHolders = std::vector<std::shared_ptr<ClipInterface>>;
using ClipConstHolders = std::vector<std::shared_ptr<const ClipInterface>>;
