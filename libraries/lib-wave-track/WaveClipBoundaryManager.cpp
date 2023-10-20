#include "WaveClipBoundaryManager.h"

#include "WaveClipBoundaryManagerOwner.h"
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
    , mTrimLeft { other.mTrimLeft }
    , mTrimRight { other.mTrimRight }
{
}

void WaveClipBoundaryManager::SetSequenceOffset(double offset)
{
   mSequenceOffset = offset;
   mOwner.SetEnvelopeOffset(offset);
}

sampleCount WaveClipBoundaryManager::GetNumLollies() const
{
   // s = stretch ratio, l = lollypop index, o = sequence offset:
   // i = s*l + o
   // Find least l such that i >= I, where I is the play start sample.
   const sampleCount firstVisibleLollyIndex { std::ceil(
      (GetPlayStartTime() - mSequenceOffset) * mSampleRate /
      mOwner.GetStretchFactor()) };
   const sampleCount lastVisibleLollyIndex {
      std::ceil(
         (GetPlayEndTime() - mSequenceOffset) * mSampleRate /
         mOwner.GetStretchFactor()) -
      1
   };
}

double WaveClipBoundaryManager::GetLollyTime(sampleCount lollyIndex) const
{
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
   const auto delta = GetPlayEndSample() - sample;
   mTrimRight += delta.as_double() / mSampleRate;
}

sampleCount WaveClipBoundaryManager::GetPlayStartSample() const
{
   // Ensures that the first lollypop is always visible if `mTrimeLeft` is 0.
   return sampleCount { std::floor(
      (mSequenceOffset + mTrimLeft) * mSampleRate) };
}

sampleCount WaveClipBoundaryManager::GetPlayEndSample() const
{
   return sampleCount { std::floor(
                           mSequenceOffset * mSampleRate +
                           mOwner.GetStretchedSequenceSampleCount() -
                           mTrimRight) +
                        1 };
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

sampleCount WaveClipBoundaryManager::GetNumTrimmedSamplesRight() const
{
   return { 0 };
}

void WaveClipBoundaryManager::OnProjectTempoChange(double newToOldRatio)
{
   mSequenceOffset *= newToOldRatio;
   mTrimLeft *= newToOldRatio;
   mTrimRight *= newToOldRatio;
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
   mTrimLeft *= newToOldRatio;
   mTrimRight *= newToOldRatio;
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
   return mTrimLeft;
}

double WaveClipBoundaryManager::GetTrimRight() const
{
   return mTrimRight;
}

void WaveClipBoundaryManager::SetTrimLeft(double trim)
{
   mTrimLeft = std::clamp(
      trim, 0.,
      mOwner.GetStretchedSequenceSampleCount() / mSampleRate - mTrimRight);
}

void WaveClipBoundaryManager::SetTrimRight(double trim)
{
   mTrimRight = std::clamp(
      trim, 0.,
      mSequenceOffset + mOwner.GetStretchedSequenceSampleCount() / mSampleRate -
         mTrimLeft);
}

void WaveClipBoundaryManager::WriteXML(XMLWriter& xmlFile) const
{
   xmlFile.WriteAttr(wxT("offset"), mSequenceOffset, 8);
   xmlFile.WriteAttr(wxT("trimLeft"), mTrimLeft, 8);
   xmlFile.WriteAttr(wxT("trimRight"), mTrimRight, 8);
}

void WaveClipBoundaryManager::ShiftBy(sampleCount offset)
{
   mSequenceOffset += offset.as_double() / mSampleRate;
   mOwner.SetEnvelopeOffset(mSequenceOffset);
}
