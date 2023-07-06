/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  ClipInterface.h

  Matthieu Hodgkinson

**********************************************************************/
#pragma once

#include "AudioSegmentSampleView.h"
#include "SampleCount.h"
#include "SampleFormat.h"

class STRETCHING_SEQUENCE_API ClipInterface
{
public:
   virtual ~ClipInterface();

   virtual AudioSegmentSampleView
   GetSampleView(size_t iChannel, sampleCount start, size_t length) const = 0;

   /*!
    * The number of raw audio samples not hidden by trimming.
    */
   virtual sampleCount GetVisibleSampleCount() const = 0;

   virtual size_t GetWidth() const = 0;

   virtual int GetRate() const = 0;

   virtual double GetPlayStartTime() const = 0;

   virtual double GetPlayEndTime() const = 0;

   virtual double GetStretchRatio() const = 0;
};

using ClipHolders = std::vector<std::shared_ptr<ClipInterface>>;
using ClipConstHolders = std::vector<std::shared_ptr<const ClipInterface>>;
