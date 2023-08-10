/**********************************************************************

  Audacity: A Digital Audio Editor

  @file WaveTrackSink.h
  @brief Adapter of WaveTrack to the interface AudioGraph::Sink

  Dominic Mazzoni
  Vaughan Johnson

  Paul Licameli split from PerTrackEffect.h

**********************************************************************/

#ifndef __AUDACITY_WAVE_TRACK_SINK__
#define __AUDACITY_WAVE_TRACK_SINK__

#include "AudioGraphSink.h" // to inherit
#include "SampleCount.h"
#include "SampleFormat.h"
#include <memory>

class WaveTrack;
class TrackList;

class WAVE_TRACK_API WaveTrackSink final : public AudioGraph::Sink {
public:
   WaveTrackSink(
      WaveTrack& left, WaveTrack* pRight, sampleCount start,
      //! This argument affects processors only, not generators
      sampleFormat effectiveFormat);
   ~WaveTrackSink() override;

   //! Accepts buffers only if there is at least one channel
   bool AcceptsBuffers(const Buffers &buffers) const override;

   bool Acquire(Buffers &data) override;
   bool Release(const Buffers &data, size_t curBlockSize) override;

   /*!
    @copydoc DoConsume
    @return accumulated data from all calls to Release
    */
   std::shared_ptr<TrackList> Flush(Buffers &data);

private:
   /*!
    @pre `data.Channels() > 0`
    @post `data.BlockSize() <= data.Remaining()`
    */
   void DoConsume(Buffers &data);

   WaveTrack &mLeft;
   WaveTrack *const mpRight;
   const std::shared_ptr<WaveTrack> mGenLeft, mGenRight;
   const std::shared_ptr<TrackList> mList;
   const sampleFormat mEffectiveFormat;

   sampleCount mOutPos;
};
#endif
