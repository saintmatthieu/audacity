#pragma once

#include "AudioSegmentSampleView.h"
#include "SampleCount.h"
#include "SampleFormat.h"
#include "SeqBlock.h"
#include "XMLTagHandler.h"

class SampleBlockFactory;

using SampleBlockFactoryPtr = std::shared_ptr<SampleBlockFactory>;

// Virtual API of methods from class `Sequence` used in WaveClip.cpp
class SequenceInterface : public XMLTagHandler
{
public:
   virtual ~SequenceInterface() = default;

   virtual AudioSegmentSampleView GetFloatSampleView(
      sampleCount start, size_t length, bool mayThrow) const = 0;

   virtual bool Get(
      samplePtr buffer, sampleFormat format, sampleCount start, size_t len,
      bool mayThrow) const = 0;

   virtual void SetSamples(
      constSamplePtr buffer, sampleFormat format, sampleCount start,
      sampleCount len, sampleFormat effectiveFormat) = 0;

   virtual void SetSilence(sampleCount s0, sampleCount len) = 0;

   virtual void Paste(sampleCount s, const SequenceInterface* src) = 0;

   virtual BlockArray& GetBlockArray() = 0;
   virtual const BlockArray& GetBlockArray() const = 0;

   virtual size_t GetAppendBufferLen() const = 0;

   virtual constSamplePtr GetAppendBuffer() const = 0;

   virtual sampleCount GetNumSamples() const = 0;

   virtual SampleFormats GetSampleFormats() const = 0;

   virtual const SampleBlockFactoryPtr& GetFactory() const = 0;

   virtual std::pair<float, float>
   GetMinMax(sampleCount start, sampleCount len, bool mayThrow) const = 0;

   virtual float
   GetRMS(sampleCount start, sampleCount len, bool mayThrow) const = 0;

   virtual bool ConvertToSampleFormat(
      sampleFormat format,
      const std::function<void(size_t)>& progressReport) = 0;

   virtual bool Append(
      constSamplePtr buffer, sampleFormat format, size_t len, size_t stride,
      sampleFormat effectiveFormat) = 0;

   virtual SeqBlock::SampleBlockPtr
   AppendNewBlock(constSamplePtr buffer, sampleFormat format, size_t len) = 0;

   virtual void AppendSharedBlock(const SeqBlock::SampleBlockPtr& pBlock) = 0;

   virtual size_t GetBestBlockSize(sampleCount start) const = 0;
   virtual size_t GetMaxBlockSize() const = 0;
   virtual size_t GetIdealBlockSize() const = 0;

   virtual int FindBlock(sampleCount pos) const = 0;

   virtual void Flush() = 0;

   virtual std::unique_ptr<SequenceInterface>
   GetCopy(const SampleBlockFactoryPtr& factory) const = 0;

   virtual std::unique_ptr<SequenceInterface> GetEmptyCopy() const = 0;

   virtual void WriteXML(XMLWriter& xmlFile) const = 0;

   virtual bool GetErrorOpening() const = 0;

   virtual void InsertSilence(sampleCount s0, sampleCount len) = 0;

   virtual void Delete(sampleCount start, sampleCount len) = 0;

   virtual bool CloseLock() = 0;
};
