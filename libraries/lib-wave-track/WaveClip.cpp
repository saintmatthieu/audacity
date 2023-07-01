/**********************************************************************

  Audacity: A Digital Audio Editor

  WaveClip.cpp

  ?? Dominic Mazzoni
  ?? Markus Meyer

*******************************************************************//**

\class WaveClip
\brief This allows multiple clips to be a part of one WaveTrack.

*//*******************************************************************/
#include "WaveClip.h"

#include <math.h>
#include <optional>
#include <vector>
#include <wx/log.h>

#include "BasicUI.h"
#include "Sequence.h"
#include "Prefs.h"
#include "Envelope.h"
#include "Resample.h"
#include "InconsistencyException.h"
#include "UserException.h"

#ifdef _OPENMP
#include <omp.h>
#endif

WaveClipListener::~WaveClipListener()
{
}

WaveClip::WaveClip(size_t width,
   const SampleBlockFactoryPtr &factory,
   sampleFormat format, int rate, int colourIndex)
{
   assert(width > 0);
   mRate = rate;
   mColourIndex = colourIndex;
   mSequences.resize(width);
   for (auto &pSequence : mSequences)
      pSequence = std::make_unique<Sequence>(factory,
         SampleFormats{narrowestSampleFormat, format});

   mEnvelope = std::make_unique<Envelope>(true, 1e-7, 2.0, 1.0);
   assert(CheckInvariants());
}

WaveClip::WaveClip(
   const WaveClip& orig, const SampleBlockFactoryPtr& factory,
   bool copyCutlines)
    : mUiStretchRatio(orig.mUiStretchRatio)
    , mSequenceTempo(orig.mSequenceTempo)
{
   // essentially a copy constructor - but you must pass in the
   // current sample block factory, because we might be copying
   // from one project to another

   mSequenceOffset = orig.mSequenceOffset;
   mTrimLeft = orig.mTrimLeft;
   mTrimRight = orig.mTrimRight;
   mRate = orig.mRate;
   mColourIndex = orig.mColourIndex;
   mSequences.reserve(orig.GetWidth());
   for (auto &pSequence : orig.mSequences)
      mSequences.push_back(
         std::make_unique<Sequence>(*pSequence, factory));

   mEnvelope = std::make_unique<Envelope>(*orig.mEnvelope);

   mName = orig.mName;

   if (copyCutlines)
      for (const auto &clip: orig.mCutLines)
         mCutLines.push_back(std::make_shared<WaveClip>(*clip, factory, true));

   mIsPlaceholder = orig.GetIsPlaceholder();

   assert(GetWidth() == orig.GetWidth());
   assert(CheckInvariants());
}

WaveClip::WaveClip(
   const WaveClip& orig, const SampleBlockFactoryPtr& factory,
   bool copyCutlines, double t0, double t1, BPS origProjectTempo)
    : mUiStretchRatio(orig.mUiStretchRatio)
    , mSequenceTempo(orig.mSequenceTempo)
{
   assert(orig.CountSamples(t0, t1, origProjectTempo) > 0);

   mSequenceOffset = orig.mSequenceOffset;
   const Beat b0 { origProjectTempo.get() * t0 };
   const Beat b1 { origProjectTempo.get() * t1 };

   // The information provided in this ctor argument list are with respect to
   // the project `orig` was copied from.
   const Beat seqStartTime = orig.GetSequenceStartTime();
   const Beat playStartTime = orig.GetPlayStartTime();
   const Beat playEndTime = orig.GetPlayEndTime();
   const Beat seqDuration { origProjectTempo.get() *
                            orig.GetSequenceSamplesCount().as_double() /
                            orig.GetRate() };
   const Beat seqEndTime = seqStartTime + seqDuration;
   const Pct selectionTrimLeft { (b0 - seqStartTime).get() /
                                 seqDuration.get() };
   const Pct selectionTrimRight { (seqEndTime - playEndTime).get() /
                                  seqDuration.get() };

   //Adjust trim values to sample-boundary
   mTrimLeft = std::max(mTrimLeft, selectionTrimLeft);
   mTrimRight = std::max(mTrimRight, selectionTrimRight);

   mRate = orig.mRate;
   mColourIndex = orig.mColourIndex;

   mIsPlaceholder = orig.GetIsPlaceholder();

   mSequences.reserve(orig.GetWidth());
   for (auto &pSequence : orig.mSequences)
      mSequences.push_back(
         std::make_unique<Sequence>(*pSequence, factory));

   mEnvelope = std::make_unique<Envelope>(*orig.mEnvelope);

   if (copyCutlines)
      for (const auto &cutline : orig.mCutLines)
         mCutLines.push_back(

            std::make_shared<WaveClip>(*cutline, factory, true));

   assert(GetWidth() == orig.GetWidth());
   assert(CheckInvariants());
}


WaveClip::~WaveClip()
{
}

AudioSegmentSampleView
WaveClip::GetSampleView(size_t ii, sampleCount start, size_t length) const
{
   assert(ii < GetWidth());
   return mSequences[ii]->GetFloatSampleView(
      start + GetTrimLeftSamples(ii), length);
}

size_t WaveClip::GetWidth() const
{
   return mSequences.size();
}

bool WaveClip::GetSamples(size_t ii,
   samplePtr buffer, sampleFormat format,
   sampleCount start, size_t len, bool mayThrow) const
{
   assert(ii < GetWidth());
   return mSequences[ii]->Get(
      buffer, format, start + GetTrimLeftSamples(ii), len, mayThrow);
}

bool WaveClip::GetSamples(samplePtr buffers[], sampleFormat format,
   sampleCount start, size_t len, bool mayThrow) const
{
   bool result = true;
   for (size_t ii = 0, width = GetWidth(); result && ii < width; ++ii)
      result = GetSamples(ii, buffers[ii], format, start, len, mayThrow);
   return result;
}

/*! @excsafety{Strong} */
void WaveClip::SetSamples(size_t ii,
   constSamplePtr buffer, sampleFormat format,
   sampleCount start, size_t len, sampleFormat effectiveFormat)
{
   assert(ii < GetWidth());
   // use Strong-guarantee
   mSequences[ii]->SetSamples(
      buffer, format, start + GetTrimLeftSamples(ii), len, effectiveFormat);

   // use No-fail-guarantee
   MarkChanged();
}

BlockArray* WaveClip::GetSequenceBlockArray(size_t ii)
{
   assert(ii < GetWidth());
   return &mSequences[ii]->GetBlockArray();
}

const BlockArray* WaveClip::GetSequenceBlockArray(size_t ii) const
{
   assert(ii < GetWidth());
   return &mSequences[ii]->GetBlockArray();
}

size_t WaveClip::GetAppendBufferLen() const
{
   // All append buffers have equal lengths by class invariant
   return mSequences[0]->GetAppendBufferLen();
}

sampleCount WaveClip::GetNumSamples() const
{
   // All sequences have equal lengths by class invariant
   return mSequences[0]->GetNumSamples();
}

SampleFormats WaveClip::GetSampleFormats() const
{
   // All sequences have the same formats by class invariant
   return mSequences[0]->GetSampleFormats();
}

const SampleBlockFactoryPtr &WaveClip::GetFactory()
{
   // All sequences have the same factory by class invariant
   return mSequences[0]->GetFactory();
}

constSamplePtr WaveClip::GetAppendBuffer(size_t ii) const
{
   assert(ii < GetWidth());
   return mSequences[ii]->GetAppendBuffer();
}

void WaveClip::MarkChanged() // NOFAIL-GUARANTEE
{
   Caches::ForEach( std::mem_fn( &WaveClipListener::MarkChanged ) );
}

std::pair<float, float> WaveClip::GetMinMax(
   size_t ii, double t0, double t1, BPS bps, bool mayThrow) const
{
   assert(ii < GetWidth());
   t0 = std::max(t0, GetPlayStartTime() / bps);
   t1 = std::min(t1, GetPlayEndTime() / bps);
   if (t0 > t1) {
      if (mayThrow)
         THROW_INCONSISTENCY_EXCEPTION;
      return {
         0.f,  // harmless, but unused since Sequence::GetMinMax does not use these values
         0.f   // harmless, but unused since Sequence::GetMinMax does not use these values
      };
   }

   if (t0 == t1)
      return{ 0.f, 0.f };

   auto s0 = TimeToSequenceSamples(t0, bps);
   auto s1 = TimeToSequenceSamples(t1, bps);

   return mSequences[ii]->GetMinMax(s0, s1 - s0, mayThrow);
}

float WaveClip::GetRMS(
   size_t ii, double t0, double t1, BPS bps, bool mayThrow) const
{
   assert(ii < GetWidth());
   t0 = std::max(t0, GetPlayStartTime() / bps);
   t1 = std::min(t1, GetPlayEndTime() / bps);
   if (t0 > t1) {
      if (mayThrow)
         THROW_INCONSISTENCY_EXCEPTION;
      return 0.f;
   }

   if (t0 == t1)
      return 0.f;

   auto s0 = TimeToSequenceSamples(t0, bps);
   auto s1 = TimeToSequenceSamples(t1, bps);

   return mSequences[ii]->GetRMS(s0, s1-s0, mayThrow);
}

void WaveClip::ConvertToSampleFormat(sampleFormat format,
   const std::function<void(size_t)> & progressReport)
{
   // Note:  it is not necessary to do this recursively to cutlines.
   // They get converted as needed when they are expanded.

   Transaction transaction{ *this };

   auto bChanged = mSequences[0]->ConvertToSampleFormat(format, progressReport);
   for (size_t ii = 1, width = GetWidth(); ii < width; ++ii) {
      bool alsoChanged =
         mSequences[ii]->ConvertToSampleFormat(format, progressReport);
      // Class invariant implies:
      assert(bChanged == alsoChanged);
   }
   if (bChanged)
      MarkChanged();

   transaction.Commit();
}

/*! @excsafety{No-fail} */
void WaveClip::UpdateEnvelopeTrackLen()
{
   auto len = (GetNumSamples().as_double()) / mRate;
   if (len != mEnvelope->GetTrackLen())
      mEnvelope->SetTrackLen(len, 1.0 / GetRate());
}

/*! @excsafety{Strong} */
std::shared_ptr<SampleBlock> WaveClip::AppendNewBlock(
   samplePtr buffer, sampleFormat format, size_t len)
{
   // This is a special use function for legacy files only and this assertion
   // does not need to be relaxed
   assert(GetWidth() == 1);
   return mSequences[0]->AppendNewBlock( buffer, format, len );
}

/*! @excsafety{Strong} */
void WaveClip::AppendSharedBlock(const std::shared_ptr<SampleBlock> &pBlock)
{
   // This is a special use function for legacy files only and this assertion
   // does not need to be relaxed
   assert(GetWidth() == 1);
   mSequences[0]->AppendSharedBlock( pBlock );
}

bool WaveClip::Append(constSamplePtr buffers[], sampleFormat format,
   size_t len, unsigned int stride, sampleFormat effectiveFormat)
{
   Finally Do{ [this]{ assert(CheckInvariants()); } };

   Transaction transaction{ *this };

   //wxLogDebug(wxT("Append: len=%lli"), (long long) len);

   size_t ii = 0;
   bool appended = false;
   for (auto &pSequence : mSequences)
      appended =
         pSequence->Append(buffers[ii++], format, len, stride, effectiveFormat)
         || appended;

   transaction.Commit();
   // use No-fail-guarantee
   UpdateEnvelopeTrackLen();
   MarkChanged();

   return appended;
}

void WaveClip::Flush()
{
   //wxLogDebug(wxT("WaveClip::Flush"));
   //wxLogDebug(wxT("   mAppendBufferLen=%lli"), (long long) mAppendBufferLen);
   //wxLogDebug(wxT("   previous sample count %lli"), (long long) mSequence->GetNumSamples());

   if (GetAppendBufferLen() > 0) {

      Transaction transaction{ *this };

      for (auto &pSequence : mSequences)
         pSequence->Flush();

      transaction.Commit();

      // No-fail operations
      UpdateEnvelopeTrackLen();
      MarkChanged();
   }

   //wxLogDebug(wxT("now sample count %lli"), (long long) mSequence->GetNumSamples());
}

bool WaveClip::HandleXMLTag(const std::string_view& tag, const AttributesList &attrs)
{
   if (tag == "waveclip")
   {
      double dblValue;
      long longValue;
      for (auto pair : attrs)
      {
         auto attr = pair.first;
         auto value = pair.second;

         if (attr == "offset")
         {
            if (!value.TryGet(dblValue))
               return false;
            // todo(mhodgkinson): Here's a difficulty: compatibility.
            // When saving and loading a project, the project tempo is also
            // saved or loaded, so it exists. We will need this information to
            // convert from and to time.
            assert(false);
            // SetSequenceStartTime(dblValue);
         }
         else if (attr == "trimLeft")
         {
            if (!value.TryGet(dblValue))
               return false;
            assert(false);
            // SetTrimLeft(dblValue);
         }
         else if (attr == "trimRight")
         {
            if (!value.TryGet(dblValue))
               return false;
            assert(false);
            // SetTrimRight(dblValue);
         }
         else if (attr == "clipStretchRatio")
         {
            if (!value.TryGet(dblValue))
               return false;
            mUiStretchRatio = dblValue;
         }
         else if (attr == "sourceTempo")
         {
            if (!value.TryGet(dblValue))
               return false;
            // todo(mhodgkinson): same thing here
            assert(false);
            // mSequenceTempo = dblValue;
         }
         else if (attr == "name")
         {
            if(value.IsStringView())
               SetName(value.ToWString());
         }
         else if (attr == "colorindex")
         {
            if (!value.TryGet(longValue))
               return false;
            SetColourIndex(longValue);
         }
      }
      return true;
   }

   return false;
}

void WaveClip::HandleXMLEndTag(const std::string_view& tag)
{
   // All blocks were deserialized into new sequences; remove the one made
   // by the constructor which remains empty.
   mSequences.erase(mSequences.begin());
   mSequences.shrink_to_fit();
   if (tag == "waveclip")
      UpdateEnvelopeTrackLen();
   // A proof of this assertion assumes that nothing has happened since
   // construction of this, besides calls to the other deserialization
   // functions
   assert(CheckInvariants());
}

XMLTagHandler *WaveClip::HandleXMLChild(const std::string_view& tag)
{
   auto &pFirst = mSequences[0];
   if (tag == "sequence") {
      mSequences.push_back(std::make_unique<Sequence>(
         pFirst->GetFactory(), pFirst->GetSampleFormats()));
      return mSequences.back().get();
   }
   else if (tag == "envelope")
      return mEnvelope.get();
   else if (tag == "waveclip")
   {
      // Nested wave clips are cut lines
      auto format = pFirst->GetSampleFormats().Stored();
      // The format is not stored in WaveClip itself but passed to
      // Sequence::Sequence; but then the Sequence will deserialize format
      // again
      mCutLines.push_back(
         std::make_shared<WaveClip>(
            // Make only one channel now, but recursive deserialization
            // increases the width later
            1, pFirst->GetFactory(),
            format, mRate, 0 /*colourindex*/));
      return mCutLines.back().get();
   }
   else
      return NULL;
}

void WaveClip::WriteXML(XMLWriter &xmlFile) const
// may throw
{
   // todo(mhodgkinson) same thing here
   assert(false);

   xmlFile.StartTag(wxT("waveclip"));
   // xmlFile.WriteAttr(wxT("offset"), mSequenceOffset, 8);
   // For compatibility with older projects, convert trim values to time
   // xmlFile.WriteAttr(wxT("trimLeft"), mTrimLeft, 8);
   // xmlFile.WriteAttr(wxT("trimRight"), mTrimRight, 8);
   xmlFile.WriteAttr(wxT("clipStretchRatio"), mUiStretchRatio, 8);
   // xmlFile.WriteAttr(wxT("sourceTempo"), mSequenceTempo, 8);
   xmlFile.WriteAttr(wxT("name"), mName);
   xmlFile.WriteAttr(wxT("colorindex"), mColourIndex );

   for (auto &pSequence : mSequences)
      pSequence->WriteXML(xmlFile);
   mEnvelope->WriteXML(xmlFile);

   for (const auto &clip: mCutLines)
      clip->WriteXML(xmlFile);

   xmlFile.EndTag(wxT("waveclip"));
}

bool WaveClip::Paste(double t0, BPS projectTempo, const WaveClip& other)
{
   return Paste(Beat { t0 * projectTempo.get() }, other);
}

/*! @excsafety{Strong} */
bool WaveClip::Paste(Beat t0, const WaveClip& other)
{
   if (GetWidth() != other.GetWidth())
      return false;

   Finally Do{ [this]{ assert(CheckInvariants()); } };

   Transaction transaction{ *this };

   const bool clipNeedsResampling = other.mRate != mRate;
   const bool clipNeedsNewFormat =
      other.GetSampleFormats().Stored() != GetSampleFormats().Stored();
   std::shared_ptr<WaveClip> newClip;

   t0 = std::clamp(t0, GetPlayStartTime(), GetPlayEndTime());

   //seems like edge cases cannot happen, see WaveTrack::PasteWaveTrack
   auto &factory = GetFactory();
   if (t0 == GetPlayStartTime())
   {
      ClearSequence(GetSequenceStartTime(), t0);
      SetTrimLeft(other.GetTrimLeft());

      auto copy = std::make_shared<WaveClip>(other, factory, true);
      copy->ClearSequence(copy->GetPlayEndTime(), copy->GetSequenceEndTime());
      newClip = std::move(copy);
   }
   else if (t0 == GetPlayEndTime())
   {
      ClearSequence(GetPlayEndTime(), GetSequenceEndTime());
      SetTrimRight(other.GetTrimRight());

      auto copy = std::make_shared<WaveClip>(other, factory, true);
      copy->ClearSequence(
         copy->GetSequenceStartTime(), copy->GetPlayStartTime());
      newClip = std::move(copy);
   }
   else
   {
      newClip = std::make_shared<WaveClip>(other, factory, true);
      newClip->ClearSequence(
         newClip->GetPlayEndTime(), newClip->GetSequenceEndTime());
      newClip->ClearSequence(
         newClip->GetSequenceStartTime(), newClip->GetPlayStartTime());
      newClip->SetTrimLeft(Beat { 0 });
      newClip->SetTrimRight(Beat { 0 });
   }

   if (clipNeedsResampling || clipNeedsNewFormat)
   {
      auto copy = std::make_shared<WaveClip>(*newClip.get(), factory, true);
      if (clipNeedsResampling)
         // The other clip's rate is different from ours, so resample
          copy->Resample(mRate);
      if (clipNeedsNewFormat)
         // Force sample formats to match.
         copy->ConvertToSampleFormat(GetSampleFormats().Stored());
      newClip = std::move(copy);
   }

   // Paste cut lines contained in pasted clip
   WaveClipHolders newCutlines;
   for (const auto &cutline: newClip->mCutLines)
   {
      auto cutlineCopy = std::make_unique<WaveClip>(*cutline, factory,
         // Recursively copy cutlines of cutlines.  They don't need
         // their offsets adjusted.
         true);
      cutlineCopy->Offset(t0 - GetSequenceStartTime());
      newCutlines.push_back(std::move(cutlineCopy));
   }

   sampleCount s0 = TimeToSequenceSamples(t0);

   // Because newClip was made above as a copy of (a copy of) other
   assert(other.GetWidth() == newClip->GetWidth());
   // And other has the same width as this, so this loop is safe
   // Assume Strong-guarantee from Sequence::Paste
   for (size_t ii = 0, width = GetWidth(); ii < width; ++ii)
      mSequences[ii]->Paste(s0, newClip->mSequences[ii].get());

   transaction.Commit();

   // Assume No-fail-guarantee in the remaining
   MarkChanged();
   auto sampleTime = 1.0 / GetRate();
   // todo(mhodgkinson) Not changing envelope API right now
   assert(false);
   // mEnvelope->PasteEnvelope(
   //    s0.as_double() / mRate + GetSequenceStartTime(), newClip->mEnvelope.get(),
   //    sampleTime);
   OffsetCutLines(t0, newClip->GetPlayEndTime() - newClip->GetPlayStartTime());

   for (auto &holder : newCutlines)
      mCutLines.push_back(std::move(holder));

   return true;
}

/*! @excsafety{Strong} */
void WaveClip::InsertSilence(
   double t, double len, BPS bps, double* pEnvelopeValue)
{
   Transaction transaction{ *this };

   if (t == GetPlayStartTime(bps) && t > GetSequenceStartTime(bps))
      ClearSequence(GetSequenceStartTime(bps), t, bps);
   else if (t == GetPlayEndTime(bps) && t < GetSequenceEndTime(bps))
   {
      ClearSequence(t, GetSequenceEndTime(bps), bps);
      SetTrimRight(.0, bps);
   }

   auto s0 = TimeToSequenceSamples(t, bps);
   auto slen = (sampleCount)floor(len * mRate + 0.5);

   // use Strong-guarantee
   for (auto &pSequence : mSequences)
      pSequence->InsertSilence(s0, slen);

   transaction.Commit();

   // use No-fail-guarantee
   OffsetCutLines(t, len, bps);

   const auto sampleTime = 1.0 / GetRate();
   auto pEnvelope = GetEnvelope();
   if ( pEnvelopeValue ) {

      // Preserve limit value at the end
      auto oldLen = pEnvelope->GetTrackLen();
      auto newLen = oldLen + len;
      pEnvelope->Cap( sampleTime );

      // Ramp across the silence to the given value
      pEnvelope->SetTrackLen( newLen, sampleTime );
      pEnvelope->InsertOrReplace
         ( pEnvelope->GetOffset() + newLen, *pEnvelopeValue );
   }
   else
      pEnvelope->InsertSpace( t, len );

   MarkChanged();
}

/*! @excsafety{Strong} */
void WaveClip::AppendSilence(double len, BPS bps, double envelopeValue)
{
   auto t = GetPlayEndTime(bps);
   InsertSilence(t, len, bps, &envelopeValue);
}

/*! @excsafety{Strong} */
void WaveClip::Clear(double t0, double t1, BPS bps)
{
    auto st0 = t0;
    auto st1 = t1;
    auto offset = .0;
    if (st0 <= GetPlayStartTime(bps))
    {
       offset = (t0 - GetPlayStartTime(bps)) + GetTrimLeft(bps);
       st0 = GetSequenceStartTime(bps);

       SetTrimLeft(.0, bps);
    }
    if (st1 >= GetPlayEndTime(bps))
    {
       st1 = GetSequenceEndTime(bps);
       SetTrimRight(.0, bps);
    }
    ClearSequence(st0, st1, bps);

    if (offset != .0)
        Offset(offset, bps);
}

void WaveClip::ClearLeft(double t, BPS bps)
{
   if (t > GetPlayStartTime(bps) && t < GetPlayEndTime(bps))
   {
      ClearSequence(GetSequenceStartTime(bps), t, bps);
      SetTrimLeft(.0, bps);
      SetSequenceStartTime(t, bps);
   }
}

void WaveClip::ClearRight(double t, BPS bps)
{
   if (t > GetPlayStartTime(bps) && t < GetPlayEndTime(bps))
   {
      ClearSequence(t, GetSequenceEndTime(bps), bps);
      SetTrimRight(.0, bps);
   }
}

void WaveClip::ClearSequence(double t0, double t1, BPS projectTempo)
{
   ClearSequence(
      Beat { t0 * projectTempo.get() }, Beat { t1 * projectTempo.get() });
}

void WaveClip::ClearSequence(Beat t0, Beat t1)
{
   Transaction transaction{ *this };

    auto clip_t0 = std::max(t0, GetSequenceStartTime());
    auto clip_t1 = std::min(t1, GetSequenceEndTime());

    auto s0 = TimeToSequenceSamples(clip_t0);
    auto s1 = TimeToSequenceSamples(clip_t1);

    if (s0 != s1)
    {
        // use Strong-guarantee
        for (auto &pSequence : mSequences)
           pSequence->Delete(s0, s1 - s0);

        // use No-fail-guarantee in the remaining

        // msmeyer
        //
        // Delete all cutlines that are within the given area, if any.
        //
        // Note that when cutlines are active, two functions are used:
        // Clear() and ClearAndAddCutLine(). ClearAndAddCutLine() is called
        // whenever the user directly calls a command that removes some audio, e.g.
        // "Cut" or "Clear" from the menu. This command takes care about recursive
        // preserving of cutlines within clips. Clear() is called when internal
        // operations want to remove audio. In the latter case, it is the right
        // thing to just remove all cutlines within the area.
        //

        // May DELETE as we iterate, so don't use range-for
        for (auto it = mCutLines.begin(); it != mCutLines.end();)
        {
            WaveClip* clip = it->get();
            Beat cutlinePosition =
               GetSequenceStartTime() + clip->GetSequenceStartTime();
            if (cutlinePosition >= t0 && cutlinePosition <= t1)
            {
                // This cutline is within the area, DELETE it
                it = mCutLines.erase(it);
            }
            else
            {
                if (cutlinePosition >= t1)
                {
                   clip->Offset(clip_t0 - clip_t1);
                }
                ++it;
            }
        }

        // Collapse envelope
        auto sampleTime = 1.0 / GetRate();
        // todo(mhodkginson)
        assert(false);
        //   GetEnvelope()->CollapseRegion(t0, t1, sampleTime);
    }

    transaction.Commit();
    MarkChanged();
}

/*! @excsafety{Weak}
-- This WaveClip remains destructible in case of AudacityException.
But some cutlines may be deleted */
void WaveClip::ClearAndAddCutLine(double t0, double t1, BPS bps)
{
   if (
      t0 > GetPlayEndTime(bps) || t1 < GetPlayStartTime(bps) ||
      CountSamples(t0, t1, bps) == 0)
      return; // no samples to remove

   Transaction transaction{ *this };

   const double clip_t0 = std::max(t0, GetPlayStartTime(bps));
   const double clip_t1 = std::min(t1, GetPlayEndTime(bps));

   auto newClip = std::make_shared<WaveClip>(
      *this, GetFactory(), true, clip_t0, clip_t1, bps);
   if (t1 < GetPlayEndTime(bps))
   {
      newClip->ClearSequence(t1, newClip->GetSequenceEndTime(bps), bps);
      newClip->SetTrimRight(.0, bps);
   }
   if (t0 > GetPlayStartTime(bps))
   {
      newClip->ClearSequence(newClip->GetSequenceStartTime(bps), t0, bps);
      newClip->SetTrimLeft(.0, bps);
   }

   newClip->SetSequenceStartTime(clip_t0 - GetSequenceStartTime(bps), bps);

   // Remove cutlines from this clip that were in the selection, shift
   // left those that were after the selection
   // May DELETE as we iterate, so don't use range-for
   for (auto it = mCutLines.begin(); it != mCutLines.end();)
   {
      WaveClip* clip = it->get();
      double cutlinePosition =
         GetSequenceStartTime(bps) + clip->GetSequenceStartTime(bps);
      if (cutlinePosition >= t0 && cutlinePosition <= t1)
         it = mCutLines.erase(it);
      else
      {
         if (cutlinePosition >= t1)
         {
            clip->Offset(clip_t0 - clip_t1, bps);
         }
         ++it;
      }
   }

   // Clear actual audio data
   auto s0 = TimeToSequenceSamples(t0, bps);
   auto s1 = TimeToSequenceSamples(t1, bps);

   // use Weak-guarantee
   for (auto &pSequence : mSequences)
      pSequence->Delete(s0, s1-s0);

   // Collapse envelope
   auto sampleTime = 1.0 / GetRate();
   GetEnvelope()->CollapseRegion( t0, t1, sampleTime );

   MarkChanged();

   mCutLines.push_back(std::move(newClip));

   // New cutline was copied from this so will have correct width
   assert(CheckInvariants());
}

bool WaveClip::FindCutLine(
   double cutLinePosition, BPS bps, double* cutlineStart /* = NULL */,
   double* cutlineEnd /* = NULL */) const
{
   for (const auto &cutline: mCutLines)
   {
      if (
         fabs(
            GetSequenceStartTime(bps) + cutline->GetSequenceStartTime(bps) -
            cutLinePosition) < 0.0001)
      {
         auto startTime =
            GetSequenceStartTime(bps) + cutline->GetSequenceStartTime(bps);
         if (cutlineStart)
            *cutlineStart = startTime;
         if (cutlineEnd)
            *cutlineEnd = startTime + cutline->SamplesToTime(cutline->GetPlaySamplesCount());
         return true;
      }
   }

   return false;
}

/*! @excsafety{Strong} */
void WaveClip::ExpandCutLine(double cutLinePosition, BPS bps)
{
   auto end = mCutLines.end();
   auto it =
      std::find_if(mCutLines.begin(), end, [&](const WaveClipHolder& cutline) {
         return fabs(
                   GetSequenceStartTime(bps) +
                   cutline->GetSequenceStartTime(bps) - cutLinePosition) <
                0.0001;
      });

   if ( it != end ) {
      auto *cutline = it->get();
      // assume Strong-guarantee from Paste

      // Envelope::Paste takes offset into account, WaveClip::Paste doesn't!
      // Do this to get the right result:
      cutline->mEnvelope->SetOffset(0);

      bool success = Paste(
         GetSequenceStartTime(bps) + cutline->GetSequenceStartTime(bps), bps,
         *cutline);
      assert(success); // class invariant promises cutlines have correct width

      // Now erase the cutline,
      // but be careful to find it again, because Paste above may
      // have modified the array of cutlines (if our cutline contained
      // another cutline!), invalidating the iterator we had.
      end = mCutLines.end();
      it = std::find_if(mCutLines.begin(), end,
         [=](const WaveClipHolder &p) { return p.get() == cutline; });
      if (it != end)
         mCutLines.erase(it); // deletes cutline!
      else {
         wxASSERT(false);
      }
   }
}

bool WaveClip::RemoveCutLine(double cutLinePosition, BPS bps)
{
   for (auto it = mCutLines.begin(); it != mCutLines.end(); ++it)
   {
      const auto &cutline = *it;
      //std::numeric_limits<double>::epsilon() or (1.0 / static_cast<double>(mRate))?
      if (
         fabs(
            GetSequenceStartTime(bps) + cutline->GetSequenceStartTime(bps) -
            cutLinePosition) < 0.0001)
      {
         mCutLines.erase(it); // deletes cutline!
         return true;
      }
   }

   return false;
}

void WaveClip::OffsetCutLines(Beat t0, Beat len)
{
   for (const auto &cutLine : mCutLines)
      if (GetSequenceStartTime() + cutLine->GetSequenceStartTime() >= t0)
         cutLine->Offset(len);
}

/*! @excsafety{No-fail} */
void WaveClip::OffsetCutLines(double t0, double len, BPS bps)
{
   OffsetCutLines(Beat { t0 * bps.get() }, Beat { len * bps.get() });
}

void WaveClip::CloseLock() noexcept
{
   // Don't need a Transaction for noexcept operations
   for (auto &pSequence : mSequences)
      pSequence->CloseLock();
   for (const auto &cutline: mCutLines)
      cutline->CloseLock();
}

void WaveClip::SetRate(int rate, bool reposition)
{
   if (reposition)
   {
      const auto ratio = mRate / rate;
      SetSequenceStartTime(Beat { GetSequenceStartTime().get() * ratio });
   }
   mRate = rate;
   auto newLength = GetNumSamples().as_double() / mRate;
   mEnvelope->RescaleTimes( newLength );
   MarkChanged();
}

double WaveClip::GetStretchRatio(BPS bps) const
{
   return mUiStretchRatio * (bps / mSequenceTempo).get();
}

/*! @excsafety{Strong} */
void WaveClip::Resample(int rate, BasicUI::ProgressDialog *progress)
{
   // Note:  it is not necessary to do this recursively to cutlines.
   // They get resampled as needed when they are expanded.

   if (rate == mRate)
      return; // Nothing to do

   // This function does its own RAII without a Transaction

   double factor = (double)rate / (double)mRate;
   ::Resample resample(true, factor, factor); // constant rate resampling

   const size_t bufsize = 65536;
   Floats inBuffer{ bufsize };
   Floats outBuffer{ bufsize };
   sampleCount pos = 0;
   bool error = false;
   int outGenerated = 0;
   const auto numSamples = GetNumSamples();

   // These sequences are appended to below
   decltype(mSequences) newSequences;
   newSequences.reserve(mSequences.size());
   for (auto &pSequence : mSequences)
      newSequences.push_back(std::make_unique<Sequence>(
         pSequence->GetFactory(), pSequence->GetSampleFormats()));

   /**
    * We want to keep going as long as we have something to feed the resampler
    * with OR as long as the resampler spews out samples (which could continue
    * for a few iterations after we stop feeding it)
    */
   while (pos < numSamples || outGenerated > 0) {
      const auto inLen = limitSampleBufferSize( bufsize, numSamples - pos );

      bool isLast = ((pos + inLen) == numSamples);

      auto ppNewSequence = newSequences.begin();
      std::optional<std::pair<size_t, size_t>> results{};
      for (auto &pSequence : mSequences) {
         auto &pNewSequence = *ppNewSequence++;
         if (!pSequence->Get((samplePtr)inBuffer.get(), floatSample, pos, inLen, true))
         {
            error = true;
            break;
         }

         // Expect the same results for all channels, or else fail
         auto newResults = resample.Process(factor, inBuffer.get(), inLen,
            isLast, outBuffer.get(), bufsize);
         if (!results)
            results.emplace(newResults);
         else if (*results != newResults) {
            error = true;
            break;
         }

         outGenerated = results->second;
         if (outGenerated < 0) {
            error = true;
            break;
         }

         pNewSequence->Append((samplePtr)outBuffer.get(), floatSample,
            outGenerated, 1,
            widestSampleFormat /* computed samples need dither */
         );
      }
      if (results)
         pos += results->first;

      if (progress)
      {
         auto updateResult = progress->Poll(
            pos.as_long_long(),
            numSamples.as_long_long()
         );
         error = (updateResult != BasicUI::ProgressResult::Success);
         if (error)
            throw UserException{};
      }
   }

   if (error)
      throw SimpleMessageBoxException{
         ExceptionType::Internal,
         XO("Resampling failed."),
         XO("Warning"),
         "Error:_Resampling"
      };
   else
   {
      // Use No-fail-guarantee in these steps
      mSequences = move(newSequences);
      mRate = rate;
      Flush();
      Caches::ForEach( std::mem_fn( &WaveClipListener::Invalidate ) );
   }
}

// Used by commands which interact with clips using the keyboard.
// When two clips are immediately next to each other, the GetPlayEndTime()
// of the first clip and the GetPlayStartTime() of the second clip may not
// be exactly equal due to rounding errors.
bool WaveClip::SharesBoundaryWithNextClip(const WaveClip* next, BPS bps) const
{
   const Beat endThis = GetPlayEndTime();
   const Beat startNext = next->GetPlayStartTime();
   const auto deltaInSamples = (startNext - endThis) / bps * mRate;

   // given that a double has about 15 significant digits, using a criterion
   // of half a sample should be safe in all normal usage.
   return fabs(deltaInSamples) < 0.5;
}

void WaveClip::SetName(const wxString& name)
{
   mName = name;
}

const wxString& WaveClip::GetName() const
{
   return mName;
}

sampleCount WaveClip::TimeToSamples(double time) const noexcept
{
    return sampleCount(floor(time * mRate + 0.5));
}

double WaveClip::SamplesToTime(sampleCount s) const noexcept
{
    return s.as_double() / mRate;
}

void WaveClip::SetSilence(sampleCount offset, sampleCount length)
{
   const auto start =
      TimeToSamples(mTrimLeft.get() * GetNumSamples().as_double()) + offset;
   Transaction transaction{ *this };
   for (auto &pSequence : mSequences)
      pSequence->SetSilence(start, length);
   transaction.Commit();
   MarkChanged();
}

sampleCount WaveClip::GetSequenceSamplesCount() const
{
    return GetNumSamples() * GetWidth();
}

Beat WaveClip::GetPlayStartTime() const
{
   return Beat { mSequenceOffset.get() +
                 GetSequenceDuration().get() * mTrimLeft.get() };
}

double WaveClip::GetPlayStartTime(BPS bps) const noexcept
{
   return GetPlayStartTime() / bps;
}

void WaveClip::SetPlayStartTime(double time, BPS bps)
{
   const auto trimTime = mTrimLeft.get() * GetSequenceDuration(bps);
   SetSequenceStartTime(time - trimTime, bps);
}

Beat
WaveClip::GetPlayEndTime() const
{
   const auto numSamples = (GetNumSamples() + GetAppendBufferLen()).as_double();
   const Beat totalLength =
      mSequenceOffset + Beat { mSequenceTempo.get() * numSamples / mRate };
   return totalLength * Beat { 1 - mTrimRight.get() };
}

double WaveClip::GetPlayEndTime(BPS bps) const
{
   return GetPlayEndTime() / bps;
}

double
WaveClip::GetPlayDuration(BPS bps) const
{
   return GetPlayEndTime(bps) - GetPlayStartTime(bps);
}

sampleCount WaveClip::GetPlayStartSample(BPS bps) const
{
   return TimeToSamples(GetPlayStartTime(bps));
}

sampleCount WaveClip::GetPlayEndSample(BPS bps) const
{
    return GetPlayStartSample(bps) + GetPlaySamplesCount();
}

sampleCount WaveClip::GetPlaySamplesCount() const
{
   return sampleCount { GetNumSamples().as_double() *
                           (1 - mTrimLeft.get() - mTrimRight.get()) +
                        .5 };
}

void WaveClip::SetTrimLeft(Beat trim)
{
   mTrimLeft = Pct { std::max(.0, trim.get() / GetSequenceDuration().get()) };
}

void WaveClip::SetTrimLeft(double trim, BPS bps)
{
   SetTrimLeft(Beat { trim * bps.get() });
}

double WaveClip::GetTrimLeft(BPS bps) const noexcept
{
   return GetSequenceDuration(bps) * mTrimLeft.get();
}

Beat WaveClip::GetTrimLeft() const noexcept
{
   return Beat { GetSequenceDuration().get() * mTrimLeft.get() };
}

void WaveClip::SetTrimRight(double trim, BPS bps)
{
   SetTrimRight(Beat { trim * bps.get() });
}

void WaveClip::SetTrimRight(Beat trim)
{
   mTrimRight = Pct { std::max(.0, trim.get() / GetSequenceDuration().get()) };
}

Beat WaveClip::GetTrimRight() const noexcept
{
   return Beat { GetSequenceDuration().get() * mTrimRight.get() };
}

double WaveClip::GetTrimRight(BPS bps) const noexcept
{
   return GetSequenceDuration() / bps * mTrimRight.get();
}

void WaveClip::TrimLeft(double deltaTime, BPS bps)
{
   const Pct newTrimLeft { (GetTrimLeft(bps) + deltaTime) /
                           GetSequenceDuration(bps) };
   mTrimLeft = std::max(Pct { 0 }, newTrimLeft);
}

void WaveClip::TrimRight(double deltaTime, BPS bps)
{
   const Pct newTrimRight { (GetTrimRight(bps) + deltaTime) /
                           GetSequenceDuration(bps) };
   mTrimRight = std::max(Pct { 0 }, newTrimRight);
}

void WaveClip::TrimLeftTo(double to, BPS bps)
{
   SetTrimLeft(
      std::clamp(to, GetSequenceStartTime(bps), GetPlayEndTime(bps)) -
         GetSequenceStartTime(bps),
      bps);
}

void WaveClip::TrimRightTo(double to, BPS bps)
{
   SetTrimRight(
      GetSequenceEndTime(bps) -
         std::clamp(to, GetPlayStartTime(bps), GetSequenceEndTime(bps)),
      bps);
}

Beat WaveClip::GetSequenceStartTime() const noexcept
{
    // TS: scaled
    // JS: mSequenceOffset is the minimum value and it is returned; no clipping to 0
    return mSequenceOffset;
}

double WaveClip::GetSequenceStartTime(BPS tempo) const noexcept
{
   return GetSequenceStartTime() / tempo;
}

void WaveClip::SetSequenceStartTime(double startTime, BPS bps)
{
   mSequenceOffset = Beat { bps.get() * startTime };
   mEnvelope->SetOffset(startTime);
}

void WaveClip::SetSequenceStartTime(Beat beat)
{
   mSequenceOffset = beat;
   assert(false);
   // Need to re-implement envelope timing in terms of beats as well.
}

Beat WaveClip::GetSequenceEndTime() const
{
    const auto numSamples = GetNumSamples();
    const auto originalLengthInSeconds =
       (numSamples + GetAppendBufferLen()).as_double() / mRate;
    return GetSequenceStartTime() + Beat {
       mSequenceTempo.get() * originalLengthInSeconds * mUiStretchRatio
    };
}


double WaveClip::GetSequenceEndTime(BPS bps) const
{
    return GetSequenceEndTime() / bps;
}

sampleCount WaveClip::GetSequenceStartSample(BPS bps) const
{
   return TimeToSamples(mSequenceOffset / bps);
}

sampleCount WaveClip::GetSequenceEndSample(BPS bps) const
{
   return GetSequenceStartSample(bps) + GetNumSamples();
}

void WaveClip::StretchLeftTo(double to, BPS bps)
{
   const auto pet = GetPlayEndTime(bps);
   if (to >= pet)
      return;
   const auto oldPlayDuration = pet - GetPlayStartTime(bps);
   const auto newPlayDuration = pet - to;
   const auto ratioChange = newPlayDuration / oldPlayDuration;
   const Beat peb { bps.get() * pet };
   mSequenceOffset = peb - Beat { (peb - mSequenceOffset).get() * ratioChange };
   mTrimLeft *= Pct { ratioChange };
   mTrimRight *= Pct { ratioChange };
   mUiStretchRatio *= ratioChange;
   mEnvelope->SetOffset(mSequenceOffset / bps);
   mEnvelope->StretchBy(ratioChange);
}

void WaveClip::StretchRightTo(double to, BPS bps)
{
   const auto pst = GetPlayStartTime(bps);
   if (to <= pst)
      return;
   const auto oldPlayDuration = GetPlayEndTime(bps) - pst;
   const auto newPlayDuration = to - pst;
   const auto ratioChange = newPlayDuration / oldPlayDuration;
   mSequenceOffset = Beat { bps.get() * pst - mTrimLeft.get() * ratioChange };
   mTrimLeft = Pct { mTrimLeft.get() * ratioChange };
   mTrimRight = Pct { mTrimRight.get() * ratioChange };
   mUiStretchRatio *= ratioChange;
   mEnvelope->SetOffset(mSequenceOffset / bps);
   mEnvelope->StretchBy(ratioChange);
}

void WaveClip::Offset(double delta, BPS bps) noexcept
{
   Offset(Beat { bps.get() * delta });
}

void WaveClip::Offset(Beat delta) noexcept
{
   SetSequenceStartTime(GetSequenceStartTime() + delta);
}

// Bug 2288 allowed overlapping clips.
// This was a classic fencepost error.
// We are within the clip if start < t <= end.
// Note that BeforeClip and AfterClip must be consistent
// with this definition.
bool WaveClip::StrictlyWithinPlayRegion(double t, BPS bps) const
{
   return !BeforeOrAtPlayStartTime(t, bps) && !AfterOrAtPlayEndTime(t, bps);
}

bool WaveClip::BeforePlayStartTime(double t, BPS bps) const
{
   return t < GetPlayStartTime(bps);
}

bool WaveClip::BeforeOrAtPlayStartTime(double t, BPS bps) const
{
   return t <= GetPlayStartTime(bps);
}

bool WaveClip::AfterOrAtPlayEndTime(double t, BPS bps) const
{
   return GetPlayEndTime(bps) <= t;
}

sampleCount WaveClip::CountSamples(double t0, double t1, BPS bps) const
{
   if(t0 < t1)
   {
      t0 = std::max(t0, GetPlayStartTime(bps));
      t1 = std::min(t1, GetPlayEndTime(bps));
      const auto s0 = TimeToSamples(t0 - GetPlayStartTime(bps));
      const auto s1 = TimeToSamples(t1 - GetPlayStartTime(bps));
      return s1 - s0;
   }
   return { 0 };
}

sampleCount WaveClip::TimeToSequenceSamples(Beat t) const
{
    if (t < GetSequenceStartTime())
        return 0;
    else if (t > GetSequenceEndTime())
        return GetNumSamples();
    return sampleCount {
       GetNumSamples().as_double() *
          ((t - GetSequenceStartTime()) / GetSequenceDuration()).get() +
       .5
    };
}

sampleCount WaveClip::TimeToSequenceSamples(double t, BPS bps) const
{
   return TimeToSequenceSamples(Beat { bps.get() * t });
}

bool WaveClip::CheckInvariants() const
{
   const auto width = GetWidth();
   auto iter = mSequences.begin(),
      end = mSequences.end();
   // There must be at least one pointer
   if (iter != end) {
      // All pointers mut be non-null
      auto &pFirst = *iter++;
      if (pFirst) {
         // All sequences must have the same lengths, append buffer lengths,
         // sample formats, and sample block factory
         return
         std::all_of(iter, end, [&](decltype(pFirst) pSequence) {
            return pSequence &&
               pSequence->GetNumSamples() == pFirst->GetNumSamples() &&
               pSequence->GetAppendBufferLen() == pFirst->GetAppendBufferLen()
                  &&
               pSequence->GetSampleFormats() == pFirst->GetSampleFormats() &&
               pSequence->GetFactory() == pFirst->GetFactory();
         }) &&
         // All cut lines are non-null, satisfy the invariants, and match width
         std::all_of(mCutLines.begin(), mCutLines.end(),
         [width](const WaveClipHolder &pCutLine) {
            return pCutLine && pCutLine->GetWidth() == width &&
               pCutLine->CheckInvariants();
         });
      }
   }
   return false;
}

sampleCount WaveClip::GetTrimLeftSamples(size_t ii) const
{
   return sampleCount {
      mSequences[ii]->GetNumSamples().as_double() * mTrimLeft.get() + .5
   };
}

double WaveClip::GetSequenceDuration(BPS bps) const
{
   return GetSequenceEndTime(bps) - GetSequenceStartTime(bps);
}

Beat WaveClip::GetSequenceDuration() const
{
   return GetSequenceEndTime() - GetSequenceStartTime();
}

WaveClip::Transaction::Transaction(WaveClip &clip)
   : clip{ clip }
   , mTrimLeft{ clip.mTrimLeft }
   , mTrimRight{ clip.mTrimRight }
{
   sequences.reserve(clip.mSequences.size());
   auto &factory = clip.GetFactory();
   for (auto &pSequence : clip.mSequences)
      sequences.push_back(
         //! Does not copy un-flushed append buffer data
         std::make_unique<Sequence>(*pSequence, factory));
}

WaveClip::Transaction::~Transaction()
{
   if (!committed) {
      clip.mSequences.swap(sequences);
      clip.mTrimLeft = mTrimLeft;
      clip.mTrimRight = mTrimRight;
   }
}
