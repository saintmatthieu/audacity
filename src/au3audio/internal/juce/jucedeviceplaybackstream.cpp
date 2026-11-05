/*
* Audacity: A Digital Audio Editor
*/
#include "jucedeviceplaybackstream.h"

#include <algorithm>

#include "mixnode.h"

using namespace au::au4juce;

namespace {
//! If a worker can't keep up, or after a seek, it resumes this many chunks ahead of the device, as the Tracktion
//! backend's
constexpr size_t resyncLeadNumChunks = 2;
}

JuceDevicePlaybackStream::~JuceDevicePlaybackStream()
{
    stop();
}

std::optional<TrackNode::ReadAhead> JuceDevicePlaybackStream::trackReadAhead() const
{
    const auto chunkSize = readAheadChunkSize();
    return TrackNode::ReadAhead { chunkSize, readAheadNumChunks, resyncLeadNumChunks * chunkSize };
}

void JuceDevicePlaybackStream::startReadingAhead()
{
    for (const auto track : m_trackNodes) {
        track->startReading();
    }
}

void JuceDevicePlaybackStream::invalidateReadAhead()
{
    for (const auto track : m_trackNodes) {
        track->invalidateReadAhead();
    }
}

int64_t JuceDevicePlaybackStream::readAheadEnd() const
{
    int64_t end = 0;
    for (const auto track : m_trackNodes) {
        end = std::max(end, track->readAheadEnd());
    }
    return end;
}

size_t JuceDevicePlaybackStream::render(float* const* stereo, int64_t index, size_t frames)
{
    // In step with the device: what isn't rendered in time is skipped, not played late
    m_mixerNode->GetSamples(stereo, index, frames);
    return frames;
}
