/*
* Audacity: A Digital Audio Editor
*/
#include "juceworkerplaybackstream.h"

#include "framework/global/types/string.h"

#include "mixnode.h"

using namespace au::au3audio;

JuceWorkerPlaybackStream::~JuceWorkerPlaybackStream()
{
    stop();
}

void JuceWorkerPlaybackStream::onBuilt()
{
    // The device waits for the worker: after a seek, it resumes where the device is
    m_queue = std::make_unique<LockFreeQueueNode>(m_mixerNode, numChannels, readAheadChunkSize(), readAheadNumChunks, 0);
}

void JuceWorkerPlaybackStream::startReadingAhead()
{
    m_queue->Start();
}

void JuceWorkerPlaybackStream::invalidateReadAhead()
{
    m_queue->Invalidate();
}

int64_t JuceWorkerPlaybackStream::readAheadEnd() const
{
    // The sequences are read further ahead by their effects' latency
    return m_queue->ReadEnd().as_long_long() + m_maxLatency;
}

size_t JuceWorkerPlaybackStream::render(float* const* stereo, int64_t index, size_t frames)
{
    m_queue->Prepare(index);
    // As AudioIO: what the worker hasn't rendered yet is played late, after silence
    const auto available = m_queue->Available(index, frames);
    m_unplayedFrames += static_cast<int64_t>(frames - available);
    m_queue->GetSamples(stereo, index, available);
    return available;
}

std::string JuceWorkerPlaybackStream::dropoutDetails() const
{
    return ", " + muse::String::number(1000.0 * m_unplayedFrames / m_sampleRate, 0).toStdString()
           + " ms of silence waiting for rendering";
}

void JuceWorkerPlaybackStream::releaseRendering()
{
    // Stops the worker, before the mix it renders goes
    m_queue.reset();
    JucePlaybackStream::releaseRendering();
}
