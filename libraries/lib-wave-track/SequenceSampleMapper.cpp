#include "SequenceSampleMapper.h"

#include <algorithm>
#include <cassert>
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

int SequenceSampleMapper::GetRate() const
{
   return mRate;
}

void SequenceSampleMapper::SetRate(int rate)
{
   assert(rate > 0);
   const auto trimLeftSampleNum = TimeToSamples(mTrimLeft);
   const auto trimRightSampleNum = TimeToSamples(mTrimRight);
   auto ratio = static_cast<double>(GetRate()) / rate;
   mRate = rate;
   mTrimLeft = SamplesToTime(trimLeftSampleNum);
   mTrimRight = SamplesToTime(trimRightSampleNum);
   mSequenceOffset *= ratio;
}

void SequenceSampleMapper::OnProjectTempoChange(
   const std::optional<double>& oldTempo, double newTempo)
{
   if (!mRawAudioTempo.has_value())
      // When we have tempo detection ready (either by header-file
      // read-up or signal analysis) we can use something smarter than that. In
      // the meantime, use the tempo of the project when the clip is created as
      // source tempo.
      mRawAudioTempo = oldTempo.value_or(newTempo);

   if (oldTempo.has_value())
   {
      const auto ratioChange = *oldTempo / newTempo;
      mSequenceOffset *= ratioChange;
      mTrimLeft *= ratioChange;
      mTrimRight *= ratioChange;
   }
   mProjectTempo = newTempo;
}

void SequenceSampleMapper::StretchLeftTo(double t)
{
   const auto pet = GetPlayEndTime();
   if (t >= pet)
      return;
   const auto oldPlayDuration = pet - GetPlayStartTime();
   const auto newPlayDuration = pet - t;
   const auto ratioChange = newPlayDuration / oldPlayDuration;
   mSequenceOffset = pet - (pet - mSequenceOffset) * ratioChange;
   mTrimLeft *= ratioChange;
   mTrimRight *= ratioChange;
   mClipStretchRatio *= ratioChange;
}

void SequenceSampleMapper::StretchRightTo(double t)
{
   const auto pst = GetPlayStartTime();
   const auto oldPlayDuration = GetPlayEndTime() - pst;
   const auto newPlayDuration = t - pst;
   const auto ratioChange = newPlayDuration / oldPlayDuration;
   mSequenceOffset = pst - mTrimLeft * ratioChange;
   mTrimLeft *= ratioChange;
   mTrimRight *= ratioChange;
   mClipStretchRatio *= ratioChange;
}

void SequenceSampleMapper::RescaleTimesBy(double ratio)
{
   mSequenceOffset *= ratio;
   mTrimLeft *= ratio;
   mTrimRight *= ratio;
   mClipStretchRatio *= ratio;
}

void SequenceSampleMapper::OverwriteRate(int rate)
{
   mRate = rate;
}

void SequenceSampleMapper::OverwriteRawAudioTempo(std::optional<double> tempo)
{
   mRawAudioTempo = std::move(tempo);
}

void SequenceSampleMapper::OverwriteProjectTempo(std::optional<double> tempo)
{
   mProjectTempo = std::move(tempo);
}

void SequenceSampleMapper::OverwriteClipStretchRatio(double ratio)
{
   mClipStretchRatio = ratio;
}

std::optional<double> SequenceSampleMapper::GetRawAudioTempo() const
{
   return mRawAudioTempo;
}

std::optional<double> SequenceSampleMapper::GetProjectTempo() const
{
   return mProjectTempo;
}

double SequenceSampleMapper::GetClipStretchRatio() const
{
   return mClipStretchRatio;
}

void SequenceSampleMapper::TrimLeftTo(double t)
{
   mTrimLeft =
      std::clamp(t, SnapToTrackSample(mSequenceOffset), GetPlayEndTime()) -
      mSequenceOffset;
}

double SequenceSampleMapper::GetSequenceStartTime() const
{
   return mSequenceOffset;
}

void SequenceSampleMapper::SetSequenceStartTime(double t)
{
   mSequenceOffset = t;
}

double SequenceSampleMapper::GetSequenceEndTime() const
{
   // TODO is it really correct not to use mAppendBufferLen here?
   const auto numSamples = mNumSamples;
   double maxLen = GetSequenceStartTime() +
                   numSamples.as_double() * GetStretchRatio() / mRate;
   return maxLen;
}

double SequenceSampleMapper::GetSequenceDuration() const
{
   return GetSequenceEndTime() - GetSequenceStartTime();
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

double SequenceSampleMapper::GetTrimLeft() const
{
   return mTrimLeft;
}

void SequenceSampleMapper::SetTrimLeft(double trim)
{
   mTrimLeft = std::max(.0, trim);
}

void SequenceSampleMapper::TrimLeft(double deltaTime)
{
   SetTrimLeft(mTrimLeft + deltaTime);
}

double SequenceSampleMapper::GetTrimRight() const
{
   return mTrimRight;
}

void SequenceSampleMapper::SetTrimRight(double trim)
{
   mTrimRight = std::max(.0, trim);
}

void SequenceSampleMapper::TrimRight(double deltaTime)
{
   SetTrimRight(mTrimRight + deltaTime);
}

void SequenceSampleMapper::TrimRightTo(double t)
{
   const auto endTime = SnapToTrackSample(GetSequenceEndTime());
   mTrimRight = endTime - std::clamp(t, GetPlayStartTime(), endTime);
}

sampleCount SequenceSampleMapper::GetNumStems() const
{
   return mNumSamples - TimeToSamples(mTrimRight) - TimeToSamples(mTrimLeft);
}
