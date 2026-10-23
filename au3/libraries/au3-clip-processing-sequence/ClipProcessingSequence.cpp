/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  ClipProcessingSequence.cpp

  Matthieu Hodgkinson

**********************************************************************/
#include "ClipProcessingSequence.h"
#include "AudioSegment.h"
#include "AudioSegmentFactory.h"

#include <algorithm>
#include <cassert>

namespace {
void GetOffsetBuffer(
    float** offsetBuffer, const samplePtr* buffer, size_t numChannels,
    size_t offset)
{
    for (auto i = 0u; i < numChannels; ++i) {
        offsetBuffer[i] = reinterpret_cast<float*>(buffer[i]) + offset;
    }
}
} // namespace

ClipProcessingSequence::ClipProcessingSequence(
    const PlayableSequence& sequence, const AudioSegmentFactoryInterface& factory)
    : mSequence{sequence}
    , mSegments{factory.CreateClipAudioSegments()}
{
}

size_t ClipProcessingSequence::NChannels() const
{
    return mSequence.NChannels();
}

float ClipProcessingSequence::GetChannelVolume(int channel) const
{
    return mSequence.GetChannelVolume(channel);
}

const ChannelGroup* ClipProcessingSequence::FindChannelGroup() const
{
    return mSequence.FindChannelGroup();
}

bool ClipProcessingSequence::GetSolo() const
{
    return mSequence.GetSolo();
}

bool ClipProcessingSequence::GetMute() const
{
    return mSequence.GetMute();
}

double ClipProcessingSequence::GetStartTime() const
{
    return mSequence.GetStartTime();
}

double ClipProcessingSequence::GetEndTime() const
{
    return mSequence.GetEndTime();
}

double ClipProcessingSequence::GetRate() const
{
    return mSequence.GetRate();
}

sampleFormat ClipProcessingSequence::WidestEffectiveFormat() const
{
    return mSequence.WidestEffectiveFormat();
}

bool ClipProcessingSequence::HasTrivialEnvelope() const
{
    return mSequence.HasTrivialEnvelope();
}

void ClipProcessingSequence::GetEnvelopeValues(
    double* buffer, size_t bufferLen, double t0, bool backwards) const
{
    mSequence.GetEnvelopeValues(buffer, bufferLen, t0, backwards);
}

AudioGraph::ChannelType ClipProcessingSequence::GetChannelType() const
{
    return mSequence.GetChannelType();
}

bool ClipProcessingSequence::GetFloats(
    float* buffers[], sampleCount start, size_t len, bool backwards) const
{
    std::vector<samplePtr> charBuffers;
    const auto nChannels = NChannels();
    charBuffers.reserve(nChannels);
    for (auto i = 0u; i < nChannels; ++i) {
        charBuffers.push_back(reinterpret_cast<samplePtr>(buffers[i]));
    }
    constexpr auto iChannel = 0u;
    return DoGet(
        iChannel, nChannels, charBuffers.data(), sampleFormat::floatSample, start,
        len, backwards);
}

bool ClipProcessingSequence::DoGet(
    size_t iChannel, size_t nBuffers, const samplePtr buffers[],
    sampleFormat format, sampleCount start, size_t len, bool backwards,
    fillFormat, bool, sampleCount*) const
{
    // ClipProcessingSequence is not expected to be used for any other case.
    assert(iChannel == 0u);
    assert(format == floatSample);
    // More-than-stereo isn't supported
    assert(nBuffers <= 2);

    // Silence where there are no clips
    for (auto i = 0u; i < nBuffers; ++i) {
        std::fill_n(reinterpret_cast<float*>(buffers[i]), len, 0.f);
    }

    // The samples read: forward, from `start` (inclusive) to `start + len`;
    // backward, from `start` (exclusive) down to `start - len`.
    const auto signedLen = static_cast<sampleCount::type>(len);
    const auto first = backwards ? start - signedLen : start;
    const auto end = first + signedLen;
    const auto rate = mSequence.GetRate();
    const auto direction = backwards ? PlaybackDirection::backward : PlaybackDirection::forward;

    for (const auto& [clip, segment] : mSegments) {
        const auto clipFirst = sampleCount { clip->GetPlayStartTime() * rate + .5 };
        const auto clipEnd = clipFirst + segment->NumSamples();
        const auto from = std::max(first, clipFirst);
        const auto to = std::min(end, clipEnd);
        if (from >= to) {
            continue;
        }
        // Where in the buffers the clip's samples go, them being in reading order
        const auto offset = (backwards ? start - to : from - start).as_size_t();
        float* offsetBuffers[2] {};
        GetOffsetBuffer(offsetBuffers, buffers, nBuffers, offset);
        segment->GetFloats(
            offsetBuffers, (backwards ? to : from) - clipFirst, (to - from).as_size_t(), direction);
    }
    return true;
}

std::shared_ptr<ClipProcessingSequence> ClipProcessingSequence::Create(
    const PlayableSequence& sequence, const ClipConstHolders& clips)
{
    const AudioSegmentFactory factory { clips };
    return std::make_shared<ClipProcessingSequence>(sequence, factory);
}
