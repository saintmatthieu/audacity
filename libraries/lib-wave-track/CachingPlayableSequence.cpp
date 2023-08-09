/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  CachingPlayableSequence.cpp

  Matthieu Hodgkinson

**********************************************************************/
#include "CachingPlayableSequence.h"
#include "AudioSegmentSampleView.h"
#include "WaveTrack.h"

#include <cassert>

CachingPlayableSequence::CachingPlayableSequence(const WaveTrack& waveTrack)
    : mWaveTrack { waveTrack }
{
}

const WideSampleSequence* CachingPlayableSequence::DoGetDecorated() const
{
   return &mWaveTrack;
}

size_t CachingPlayableSequence::NChannels() const
{
   return mWaveTrack.NChannels();
}

float CachingPlayableSequence::GetChannelGain(int channel) const
{
   return mWaveTrack.GetChannelGain(channel);
}

bool CachingPlayableSequence::Get(size_t iChannel, size_t nBuffers,
   const samplePtr buffers[], sampleFormat format, sampleCount start,
   size_t len, bool backwards, fillFormat fill,
   bool mayThrow, sampleCount* pNumWithinClips) const
{
  // CachingPlayableSequence is only used for spectral display, which doesn't
  // need backward access. Leaving it unimplemented at least for now.
  assert(!backwards);
  assert(iChannel + nBuffers <= mWaveTrack.NChannels());
  mCacheHolders =
      mWaveTrack.GetSampleView(iChannel, nBuffers, start, len, backwards);
  assert(mCacheHolders.size() == nBuffers);
  for (auto i = 0u; i < mCacheHolders.size(); ++i)
    FillBufferFromTrackBlockSequence(
        mCacheHolders[i], reinterpret_cast<float *>(buffers[i]), len);
  return !mCacheHolders.empty();
}

double CachingPlayableSequence::GetStartTime() const
{
   return mWaveTrack.GetStartTime();
}

double CachingPlayableSequence::GetEndTime() const
{
   return mWaveTrack.GetEndTime();
}

double CachingPlayableSequence::GetRate() const
{
   return mWaveTrack.GetRate();
}

sampleFormat CachingPlayableSequence::WidestEffectiveFormat() const
{
   return mWaveTrack.WidestEffectiveFormat();
}

bool CachingPlayableSequence::HasTrivialEnvelope() const
{
   return mWaveTrack.HasTrivialEnvelope();
}

void CachingPlayableSequence::GetEnvelopeValues(
   double* buffer, size_t bufferLen, double t0, bool backwards) const
{
   mWaveTrack.GetEnvelopeValues(buffer, bufferLen, t0, backwards);
}

AudioGraph::ChannelType CachingPlayableSequence::GetChannelType() const
{
   return mWaveTrack.GetChannelType();
}

bool CachingPlayableSequence::IsLeader() const
{
   return mWaveTrack.IsLeader();
}

bool CachingPlayableSequence::GetSolo() const
{
   return mWaveTrack.GetSolo();
}

bool CachingPlayableSequence::GetMute() const
{
   return mWaveTrack.GetMute();
}
