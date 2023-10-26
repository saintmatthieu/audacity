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

sampleCount WaveClipBoundaryManager::GetNumStems() const
{
   // s = stretch ratio, l = stem index, o = sequence offset:
   // i = s*l + o
   // Find least l such that i >= I, where I is the play start sample.
   const auto stretchRatio = mOwner.GetStretchFactor();
   return GetLastStemIndex(stretchRatio) - GetFirstStemIndex(stretchRatio) + 1;
}

double WaveClipBoundaryManager::GetStemTime(sampleCount stemIndex) const
{
   const auto stretchRatio = mOwner.GetStretchFactor();
   const auto sampleIndex = stemIndex + GetFirstStemIndex(stretchRatio);
   return sampleIndex.as_double() * stretchRatio / mSampleRate;
}

double WaveClipBoundaryManager::GetSequenceOffset() const
{
   return mSequenceOffset;
}

void WaveClipBoundaryManager::TrimLeftTo(sampleCount sample)
{
   mTrimLeft = sample.as_double() / mSampleRate - mSequenceOffset;
}

void WaveClipBoundaryManager::TrimRightTo(sampleCount sample)
{
   mTrimRight = mSequenceOffset +
                GetStretchedSequenceSampleCount() / mSampleRate -
                sample.as_double() / mSampleRate;
}

sampleCount WaveClipBoundaryManager::GetPlayStartSample() const
{
   // Ensures that the first stem is always visible if `mTrimeLeft` is 0.
   return sampleCount { std::floor(
      (mSequenceOffset + mTrimLeft) * mSampleRate) };
}

sampleCount WaveClipBoundaryManager::GetPlayEndSample() const
{
   return sampleCount { std::floor(
                           mSequenceOffset * mSampleRate +
                           GetStretchedSequenceSampleCount() - mTrimRight) +
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

sampleCount WaveClipBoundaryManager::GetFirstStemIndex() const
{
   return GetFirstStemIndex(mOwner.GetStretchFactor());
}

sampleCount
WaveClipBoundaryManager::GetFirstStemIndex(double stretchRatio) const
{
   return sampleCount { std::ceil(
      (GetPlayStartTime() - mSequenceOffset) * mSampleRate / stretchRatio) };
}

sampleCount WaveClipBoundaryManager::GetLastStemIndex() const
{
   return GetLastStemIndex(mOwner.GetStretchFactor());
}

sampleCount WaveClipBoundaryManager::GetLastStemIndex(double stretchRatio) const
{
   return sampleCount { std::ceil(
                           (GetPlayEndTime() - mSequenceOffset) * mSampleRate /
                           stretchRatio) -
                        1 };
}

double WaveClipBoundaryManager::GetStretchedSequenceSampleCount() const
{
   return mOwner.GetSequenceSampleCount().as_double() *
          mOwner.GetStretchFactor();
}

sampleCount WaveClipBoundaryManager::GetNumLeadingHiddenStems() const
{
   return GetFirstStemIndex();
}

sampleCount WaveClipBoundaryManager::GetNumTrailingHiddenStems() const
{
   return mOwner.GetSequenceSampleCount() - GetLastStemIndex() - 1;
}

void WaveClipBoundaryManager::OnProjectTempoChange(double newToOldRatio)
{
   const auto newPlayStartTime = GetPlayStartTime() * newToOldRatio;
   RescaleAround(newPlayStartTime, newToOldRatio);
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

void WaveClipBoundaryManager::GetSequenceSampleIndices(
   sampleCount* where, size_t len, double t0, double secondsPerJump) const
{
   const auto s = mOwner.GetStretchFactor();
   for (auto i = 0; i < len; ++i)
   {
      const auto t = t0 + i * secondsPerJump;
      where[i] =
         sampleCount { std::floor((t - mSequenceOffset) * mSampleRate / s) };
   }
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
   mSequenceOffset = origin + (mSequenceOffset - origin) * newToOldRatio;
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
      trim, 0., GetStretchedSequenceSampleCount() / mSampleRate - mTrimRight);
}

void WaveClipBoundaryManager::SetTrimRight(double trim)
{
   mTrimRight = std::clamp(
      trim, 0.,
      mSequenceOffset + GetStretchedSequenceSampleCount() / mSampleRate -
         mTrimLeft);
}

void WaveClipBoundaryManager::ShiftPlayStartSampleTo(sampleCount sample)
{
   // Clip-dragging only shifts by an integer number of samples.
   ShiftBy(sample - GetPlayStartSample());
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
