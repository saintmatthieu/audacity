#pragma once

#include "WaveTrack.h"

class STRETCHING_SAMPLE_TRACK_API StretchingSampleTrack final :
    public SampleTrack
{
public:
   StretchingSampleTrack(std::shared_ptr<WaveTrack>);

   // Track
   Intervals GetIntervals() override;
   ConstIntervals GetIntervals() const override;
   void OnOwnerChange(const std::shared_ptr<TrackList>&) override;
   ChannelType GetChannel() const override;
   void SetOffset(double) override;
   void SetPan(float) override;
   void SetPanFromChannelType() override;
   void SyncLockAdjust(double, double) override;
   bool GetErrorOpening() override;
   void OnProjectTempoChange(double, double) override;
   Holder PasteInto(AudacityProject& project) const override;
   double GetOffset() const override;
   Holder Cut(double t0, double t1) override;
   Holder Copy(double t0, double t1, bool forClipboard) const override;
   void Clear(double t0, double t1) override;
   void Paste(double t0, const Track* src) override;
   void Silence(double t0, double t1) override;
   void InsertSilence(double t0, double t1) override;
   Holder Clone() const override;
   double GetStartTime() const override;
   double GetEndTime() const override;

   // SampleTrack
   sampleFormat GetSampleFormat() const override;
   ChannelType GetChannelIgnoringPan() const override;
   double GetRate() const override;
   sampleFormat WidestEffectiveFormat() const override;
   bool HasTrivialEnvelope() const override;
   void GetEnvelopeValues(
      double* buffer, size_t bufferLen, double t0) const override;
   float GetChannelGain(int channel) const override;
   size_t GetBestBlockSize(sampleCount t) const override;
   size_t GetMaxBlockSize() const override;
   sampleCount GetBlockStart(sampleCount t) const override;
   bool GetFloatsStretched(
      float* const* buffer, size_t numChannels,
      size_t samplesPerChannel) const override;
   bool Get(
      samplePtr buffer, sampleFormat format, sampleCount start, size_t len,
      fillFormat fill = fillZero, bool mayThrow = true,
      sampleCount* pNumWithinClips = nullptr) const override;

   // XMLTagHandler
   bool HandleXMLTag(
      const std::string_view& tag, const AttributesList& attrs) override;
   void HandleXMLEndTag(const std::string_view& tag) override;
   XMLTagHandler* HandleXMLChild(const std::string_view& tag) override;
   void WriteXML(XMLWriter& xmlFile) const override;

private:
   const std::shared_ptr<WaveTrack> mWaveTrack;
};
