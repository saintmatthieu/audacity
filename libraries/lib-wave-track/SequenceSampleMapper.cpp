#include "SequenceSampleMapper.h"

#include <cmath>

SequenceSampleMapper::SequenceSampleMapper(int rate)
    : mRate { rate }
{
}

SequenceSampleMapper::SequenceSampleMapper(const SequenceSampleMapper& orig)
    : mSequenceOffset { orig.mSequenceOffset }
    , mTrimLeft { orig.mTrimLeft }
    , mTrimRight { orig.mTrimRight }
    , mClipStretchRatio { orig.mClipStretchRatio }
    , mRawAudioTempo { orig.mRawAudioTempo }
    , mProjectTempo { orig.mProjectTempo }
    , mRate { orig.mRate }
{
}

SequenceSampleMapper::SequenceSampleMapper(
   const SequenceSampleMapper& orig, double t0, double t1)
    : mSequenceOffset { orig.mSequenceOffset }

    , mTrimLeft { orig.mTrimLeft }
    , mTrimRight { orig.mTrimRight }
    , mClipStretchRatio { orig.mClipStretchRatio }
    , mRawAudioTempo { orig.mRawAudioTempo }
    , mProjectTempo { orig.mProjectTempo }
    , mRate { orig.mRate }
{
   if (t0 > orig.GetPlayStartTime())
   {
      const auto s0 = orig.TimeToSamples(t0 - orig.GetSequenceStartTime());
      mTrimLeft = orig.SamplesToTime(s0);
   }

   if (t1 < orig.GetPlayEndTime())
   {
      const auto s1 = orig.TimeToSamples(orig.GetSequenceEndTime() - t1);
      mTrimRight = orig.SamplesToTime(s1);
   }
}

double SequenceSampleMapper::GetStretchRatio() const
{
   const auto dstSrcRatio =
      mProjectTempo.has_value() && mRawAudioTempo.has_value() ?
         *mRawAudioTempo / *mProjectTempo :
         1.0;
   return mClipStretchRatio * dstSrcRatio;
}

sampleCount SequenceSampleMapper::TimeToSamples(double t) const
{
   return sampleCount(floor(t * mRate / GetStretchRatio() + 0.5));
}

double SequenceSampleMapper::SamplesToTime(sampleCount s) const
{
   return s.as_double() * GetStretchRatio() / mRate;
}

double SequenceSampleMapper::SnapToTrackSample(double t) const
{
   return std::round(t * mRate) / mRate;
}

double SequenceSampleMapper::GetSequenceStartTime() const
{
   return mSequenceOffset;
}

double SequenceSampleMapper::GetSequenceEndTime() const
{
   // TODO is it really correct not to use mAppendBufferLen here?
   const auto numSamples = mNumSamples;
   double maxLen = GetSequenceStartTime() +
                   numSamples.as_double() * GetStretchRatio() / mRate;
   return maxLen;
}

double SequenceSampleMapper::GetPlayStartTime() const
{
   return SnapToTrackSample(mSequenceOffset + mTrimLeft);
}

double SequenceSampleMapper::GetPlayEndTime() const
{
   double maxLen = mSequenceOffset +
                   ((mNumSamples + mAppendBufferLen).as_double()) *
                      GetStretchRatio() / mRate -
                   mTrimRight;
   return SnapToTrackSample(maxLen);
}
