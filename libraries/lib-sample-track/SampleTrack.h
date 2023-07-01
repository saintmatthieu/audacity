/**********************************************************************

Audacity: A Digital Audio Editor

SampleTrack.h
@brief abstract Track sub-type that maps times to sample values

Paul Licameli split from WaveTrack.h

**********************************************************************/

#ifndef __AUDACITY_SAMPLE_TRACK__
#define __AUDACITY_SAMPLE_TRACK__

#include "AudioIOSequences.h"
#include "PlayableTrack.h"
#include "SampleCount.h"
#include "SampleFormat.h"

enum class sampleFormat : unsigned;

class SampleTrack;

using SampleTrackAttachments = ClientData::Site<
   SampleTrack,
   ClientData::Cloneable< ClientData::UniquePtr >,
   ClientData::DeepCopying
>;

class SAMPLE_TRACK_API SampleTrack /* not final */
   : public PlayableTrack
   , public SampleTrackAttachments
   , public PlayableSequence
{
public:
   using Attachments = SampleTrackAttachments;

   SampleTrack();
   SampleTrack(const SampleTrack &other, ProtectedCreationArg&&);
   ~SampleTrack() override;

   // Fix the otherwise ambiguous lookup of these virtual function names
   using Track::GetStartTime;
   using Track::GetEndTime;

   const TypeInfo &GetTypeInfo() const override;
   static const TypeInfo &ClassTypeInfo();

   virtual sampleFormat GetSampleFormat() const = 0;

   using WideSampleSequence::GetFloats;

   //! "narrow" overload fetches first channel only
   bool GetFloats(
      float* buffer, sampleCount start, size_t len, BPS tempo,
      fillFormat fill = fillZero, bool mayThrow = true,
      sampleCount* pNumWithinClips = nullptr) const
   {
      constexpr auto backwards = false;
      return GetFloats(
         0, 1, &buffer, start, len, tempo, backwards, fill, mayThrow,
         pNumWithinClips);
   }
};

ENUMERATE_TRACK_TYPE(SampleTrack)

class SAMPLE_TRACK_API WritableSampleTrack /* not final */
   : public SampleTrack
   , public RecordableSequence
{
public:
   WritableSampleTrack();
   WritableSampleTrack(
      const WritableSampleTrack &other, ProtectedCreationArg&&);
   ~WritableSampleTrack() override;

   // Resolve lookup ambiguity
   using Track::IsLeader;

   // Needed to resolve ambiguity with WideSampleSequence::GetRate, when this
   // abstract interface is used directly.
   // Expect the concrete subclass to define a common override for them.
   using RecordableSequence::GetRate;

   const TypeInfo &GetTypeInfo() const override;
   static const TypeInfo &ClassTypeInfo();
};

ENUMERATE_TRACK_TYPE(WritableSampleTrack)

#endif
