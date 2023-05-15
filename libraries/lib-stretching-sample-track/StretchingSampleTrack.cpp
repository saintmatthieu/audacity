#include "StretchingSampleTrack.h"

StretchingSampleTrack::StretchingSampleTrack(
   std::shared_ptr<WaveTrack> waveTrack)
    : mWaveTrack(std::move(waveTrack))
{
}

Track::Intervals StretchingSampleTrack::GetIntervals()
{
   return mWaveTrack->GetIntervals();
}

Track::ConstIntervals StretchingSampleTrack::GetIntervals() const
{
   return const_cast<const WaveTrack*>(mWaveTrack.get())->GetIntervals();
}

void StretchingSampleTrack::OnOwnerChange(
   const std::shared_ptr<TrackList>& trackList)
{
   return mWaveTrack->OnOwnerChange(trackList);
}

Track::ChannelType StretchingSampleTrack::GetChannel() const
{
   return mWaveTrack->GetChannel();
}

void StretchingSampleTrack::SetOffset(double o)
{
   return mWaveTrack->SetOffset(o);
}

void StretchingSampleTrack::SetPan(float newPan)
{
   return mWaveTrack->SetPan(newPan);
}

void StretchingSampleTrack::SetPanFromChannelType()
{
}

void StretchingSampleTrack::SyncLockAdjust(double oldT1, double newT1)
{
   return mWaveTrack->SyncLockAdjust(oldT1, newT1);
}

bool StretchingSampleTrack::GetErrorOpening()
{
   return mWaveTrack->GetErrorOpening();
}

void StretchingSampleTrack::OnProjectTempoChange(
   double oldTempo, double newTempo)
{
   return mWaveTrack->OnProjectTempoChange(oldTempo, newTempo);
}

Track::Holder StretchingSampleTrack::PasteInto(AudacityProject& project) const
{
   return mWaveTrack->PasteInto(project);
}

double StretchingSampleTrack::GetOffset() const
{
   return mWaveTrack->GetOffset();
}

Track::Holder StretchingSampleTrack::Cut(double t0, double t1)
{
   return mWaveTrack->Cut(t0, t1);
}

Track::Holder
StretchingSampleTrack::Copy(double t0, double t1, bool forClipboard) const
{
   return mWaveTrack->Copy(t0, t1, forClipboard);
}

void StretchingSampleTrack::Clear(double t0, double t1)
{
   return mWaveTrack->Clear(t0, t1);
}

void StretchingSampleTrack::Paste(double t0, const Track* src)
{
   return mWaveTrack->Paste(t0, src);
}

void StretchingSampleTrack::Silence(double t0, double t1)
{
   return mWaveTrack->Silence(t0, t1);
}

void StretchingSampleTrack::InsertSilence(double t0, double t1)
{
   return mWaveTrack->InsertSilence(t0, t1);
}

Track::Holder StretchingSampleTrack::Clone() const
{
   return mWaveTrack->Clone();
}

double StretchingSampleTrack::GetStartTime() const
{
   return mWaveTrack->GetStartTime();
}

double StretchingSampleTrack::GetEndTime() const
{
   return mWaveTrack->GetEndTime();
}

sampleFormat StretchingSampleTrack::GetSampleFormat() const
{
   return mWaveTrack->GetSampleFormat();
}

Track::ChannelType StretchingSampleTrack::GetChannelIgnoringPan() const
{
   return mWaveTrack->GetChannelIgnoringPan();
}

double StretchingSampleTrack::GetRate() const
{
   return mWaveTrack->GetRate();
}

sampleFormat StretchingSampleTrack::WidestEffectiveFormat() const
{
   return mWaveTrack->WidestEffectiveFormat();
}

bool StretchingSampleTrack::HasTrivialEnvelope() const
{
   return mWaveTrack->HasTrivialEnvelope();
}

void StretchingSampleTrack::GetEnvelopeValues(
   double* buffer, size_t bufferLen, double t0) const
{
   return mWaveTrack->GetEnvelopeValues(buffer, bufferLen, t0);
}

float StretchingSampleTrack::GetChannelGain(int channel) const
{
   return mWaveTrack->GetChannelGain(channel);
}

size_t StretchingSampleTrack::GetBestBlockSize(sampleCount t) const
{
   return mWaveTrack->GetBestBlockSize(t);
}

size_t StretchingSampleTrack::GetMaxBlockSize() const
{
   return mWaveTrack->GetMaxBlockSize();
}

sampleCount StretchingSampleTrack::GetBlockStart(sampleCount t) const
{
   return mWaveTrack->GetBlockStart(t);
}

bool StretchingSampleTrack::GetFloatsStretched(
   float* const* buffer, size_t numChannels, size_t samplesPerChannel) const
{
   return mWaveTrack->GetFloatsStretched(
      buffer, numChannels, samplesPerChannel);
}

bool StretchingSampleTrack::Get(
   samplePtr buffer, sampleFormat format, sampleCount start, size_t len,
   fillFormat fill, bool mayThrow, sampleCount* pNumWithinClips) const
{
   return mWaveTrack->Get(
      buffer, format, start, len, fill, mayThrow, pNumWithinClips);
}

bool StretchingSampleTrack::Append(
   constSamplePtr buffer, sampleFormat format, size_t len, unsigned int stride,
   sampleFormat effectiveFormat)
{
   return mWaveTrack->Append(buffer, format, len, stride, effectiveFormat);
}

void StretchingSampleTrack::Flush()
{
   return mWaveTrack->Flush();
}

bool StretchingSampleTrack::HandleXMLTag(
   const std::string_view& tag, const AttributesList& attrs)
{
   return mWaveTrack->HandleXMLTag(tag, attrs);
}

void StretchingSampleTrack::HandleXMLEndTag(const std::string_view& tag)
{
   return mWaveTrack->HandleXMLEndTag(tag);
}

XMLTagHandler*
StretchingSampleTrack::HandleXMLChild(const std::string_view& tag)
{
   return mWaveTrack->HandleXMLChild(tag);
}

void StretchingSampleTrack::WriteXML(XMLWriter& xmlFile) const
{
   return mWaveTrack->WriteXML(xmlFile);
}
