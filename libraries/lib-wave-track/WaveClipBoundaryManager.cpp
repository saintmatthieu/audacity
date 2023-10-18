#include "WaveClipBoundaryManager.h"

#include "Envelope.h"
#include "XMLWriter.h"

WaveClipBoundaryManager::WaveClipBoundaryManager(
   WaveClipBoundaryManagerOwner& owner, int sampleRate)
    : mOwner { owner }
    , mSampleRate { sampleRate }
{
}

WaveClipBoundaryManager::WaveClipBoundaryManager(
   WaveClipBoundaryManagerOwner& owner, const WaveClipBoundaryManager& other)
    : mOwner { owner }
    , mSampleRate { other.mSampleRate }
    , mSequenceOffset { other.mSequenceOffset }
    , mPlayStartTime { other.mPlayStartTime }
    , mPlayEndTime { other.mPlayEndTime }
{
}

void WaveClipBoundaryManager::SetSequenceOffset(double offset)
{
   // This operation is a shift that's not through dragging, hence us not
   // reusing ShiftTo.
   const auto delta = offset - mSequenceOffset;
   mSequenceOffset = offset;
   mPlayStartTime += delta;
   mPlayEndTime += delta;
   mOwner.SetEnvelopeOffset(offset);
}

double WaveClipBoundaryManager::GetSequenceOffset() const
{
   return mSequenceOffset;
}

void WaveClipBoundaryManager::DragPlayStartSampleTo(
   sampleCount newPlayStartSample)
{
   // Clip-dragging only shifts by an integer number of samples.
   ShiftBy(newPlayStartSample - GetPlayStartSample());
}

void WaveClipBoundaryManager::SetPlayEndSample(sampleCount sample)
{
   mPlayEndTime = sample.as_double() / mSampleRate;
}

sampleCount WaveClipBoundaryManager::GetPlayStartSample() const
{
   return sampleCount { std::floor(mPlayStartTime * mSampleRate) };
}

sampleCount WaveClipBoundaryManager::GetPlayEndSample() const
{
   return sampleCount { std::ceil(mPlayEndTime * mSampleRate) };
}

double WaveClipBoundaryManager::GetPlayStartTime() const
{
   return GetPlayStartSample().as_double() / mSampleRate;
}

double WaveClipBoundaryManager::GetPlayEndTime() const
{
   return GetPlayEndSample().as_double() / mSampleRate;
}

sampleCount WaveClipBoundaryManager::GetNumTrimmedSamplesLeft() const
{
   // It can be that the play start time is before the first sequence sample,
   // yet by less than one sample period, i.e., `GetPlayStartSample() -
   // mSequenceOffset * mRate > -1`. Hence, `GetPlayStartSample() -
   // floor(mSequenceOffset * mRate) >= 0`.
   const auto numHiddenSamples =
      GetPlayStartSample() -
      sampleCount { std::floor(mSequenceOffset * mSampleRate) };
   assert(numHiddenSamples >= 0);
   return numHiddenSamples;
}

sampleCount WaveClipBoundaryManager::GetNumTrimmedSamplesRight(
   sampleCount sequenceSampleCount, double stretchRatio) const
{
   return { 0 };
}

void WaveClipBoundaryManager::OnProjectTempoChange(double newToOldRatio)
{
   mSequenceOffset *= newToOldRatio;
   mPlayStartTime *= newToOldRatio;
   mPlayEndTime *= newToOldRatio;
   mOwner.RescaleEnvelopeTimesBy(newToOldRatio);
}

double
WaveClipBoundaryManager::GetRatioChangeWhenStretchingLeftTo(double to) const
{
   // Left- and right-stretching is done with stretch handles, and these are
   // located at the boundaries GetPlayStartTime() and GetPlayEndTime(). Hence
   // use the quantized start and end times.
   const auto playEndTime = GetPlayEndTime();
   const auto oldPlayDuration = playEndTime - GetPlayStartTime();
   const auto newPlayDuration = playEndTime - to;
   return newPlayDuration / oldPlayDuration;
}

double
WaveClipBoundaryManager::GetRatioChangeWhenStretchingRightTo(double to) const
{
   // Left- and right-stretching is done with stretch handles, and these are
   // located at the boundaries GetPlayStartTime() and GetPlayEndTime(). Hence
   // use the quantized start and end times.
   const auto playStartTime = GetPlayStartTime();
   const auto oldPlayDuration = GetPlayEndTime() - playStartTime;
   const auto newPlayDuration = to - playStartTime;
   return newPlayDuration / oldPlayDuration;
}

void WaveClipBoundaryManager::StretchFromLeft(double newToOldRatio)
{
   // Stretch such that the quantized play end time remains unchanged. The true
   // play end time values may change, though.
   RescaleAround(GetPlayEndTime(), newToOldRatio);
}

void WaveClipBoundaryManager::StretchFromRight(double newToOldRatio)
{
   // Stretch such that the quantized play start time remains unchanged. The
   // true play end time values may change, though.
   RescaleAround(GetPlayStartTime(), newToOldRatio);
}

void WaveClipBoundaryManager::RescaleAround(double origin, double newToOldRatio)
{
   mSequenceOffset += (mSequenceOffset - origin) * newToOldRatio;
   mPlayStartTime += (mPlayStartTime - origin) * newToOldRatio;
   mPlayEndTime += (mPlayEndTime - origin) * newToOldRatio;
   mOwner.SetEnvelopeOffset(mSequenceOffset);
   mOwner.RescaleEnvelopeTimesBy(newToOldRatio);
}

void WaveClipBoundaryManager::ChangeSampleRate(double newSampleRate)
{
   const auto ratio = newSampleRate / mSampleRate;
   mSampleRate = newSampleRate;
   RescaleAround(0, ratio);
}

double WaveClipBoundaryManager::GetTrimLeft() const
{
   return mPlayStartTime - mSequenceOffset;
}

double WaveClipBoundaryManager::GetTrimRight(
   sampleCount sequenceSampleCount, double stretchRatio) const
{
   return mSequenceOffset +
          sequenceSampleCount.as_double() * stretchRatio / mSampleRate -
          mPlayEndTime;
}

void WaveClipBoundaryManager::SetTrimLeft(double trim)
{
   mPlayStartTime =
      std::clamp(mSequenceOffset + trim, mSequenceOffset, mPlayEndTime);
}

void WaveClipBoundaryManager::SetTrimRight(
   double trim, sampleCount sequenceSampleCount, double stretchRatio)
{
   mPlayEndTime = std::clamp(
      mSequenceOffset + trim, mPlayStartTime,
      mSequenceOffset +
         sequenceSampleCount.as_double() * stretchRatio / mSampleRate);
}

void WaveClipBoundaryManager::WriteXML(
   XMLWriter& xmlFile, sampleCount sequenceSampleCount,
   double stretchRatio) const
{
   xmlFile.WriteAttr(wxT("offset"), mSequenceOffset);
   xmlFile.WriteAttr(wxT("playStart"), mPlayStartTime);
   xmlFile.WriteAttr(wxT("playEnd"), mPlayEndTime);

   xmlFile.WriteAttr(wxT("offset"), mSequenceOffset, 8);
   xmlFile.WriteAttr(wxT("trimLeft"), mPlayStartTime - mSequenceOffset, 8);
   xmlFile.WriteAttr(
      wxT("trimRight"),
      mSequenceOffset +
         sequenceSampleCount.as_double() / mSampleRate * stretchRatio,
      8);
}

void WaveClipBoundaryManager::ShiftBy(sampleCount offset)
{
   const auto delta = offset.as_double() / mSampleRate;
   mSequenceOffset += delta;
   mPlayStartTime += delta;
   mPlayEndTime += delta;
   mOwner.SetEnvelopeOffset(mSequenceOffset);
}
