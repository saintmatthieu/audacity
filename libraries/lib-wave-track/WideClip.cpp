/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  WideClip.cpp

  Matthieu Hodgkinson

**********************************************************************/
#include "WideClip.h"

WideClip::WideClip(
   std::shared_ptr<ClipInterface> left, std::shared_ptr<ClipInterface> right)
    : mChannels { std::move(left), std::move(right) }
{
}

AudioSegmentSampleView
WideClip::GetSampleView(size_t ii, sampleCount start, size_t len) const
{
   return mChannels[ii]->GetSampleView(0u, start, len);
}

sampleCount WideClip::GetVisibleSampleCount() const
{
   return mChannels[0u]->GetVisibleSampleCount();
}

size_t WideClip::GetWidth() const
{
   return mChannels[1u] == nullptr ? 1u : 2u;
}

int WideClip::GetRate() const
{
   return mChannels[0u]->GetRate();
}

double WideClip::GetPlayStartTime() const
{
   return mChannels[0u]->GetPlayStartTime();
}

double WideClip::GetPlayEndTime() const
{
   return mChannels[0u]->GetPlayEndTime();
}

double WideClip::GetStretchRatio() const
{
   return mChannels[0u]->GetStretchRatio();
}
