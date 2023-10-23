#pragma once

#include "ClipInterface.h"

class ClipInterfacePartialImpl : public ClipInterface
{
protected:
   ClipInterfacePartialImpl(const ClipInterface& clip)
       : mClip { clip }
   {
   }

   int GetRate() const override
   {
      return mClip.GetRate();
   }

   double GetPlayStartTime() const override
   {
      return mClip.GetPlayStartTime();
   }

   double GetPlayEndTime() const override
   {
      return mClip.GetPlayEndTime();
   }

   sampleCount TimeToSamples(double time) const override
   {
      return mClip.TimeToSamples(time);
   }

   double GetStretchRatio() const override
   {
      return mClip.GetStretchRatio();
   }

   size_t GetWidth() const override
   {
      return mClip.GetWidth();
   }

   const ClipInterface& mClip;
};
