/*
* Audacity: A Digital Audio Editor
*/
#include "juceworkerplaybackstream.h"

#include <algorithm>
#include <cmath>

#include "framework/global/containers.h"


#include "effectslatency.h"
#include "mixnode.h"
#include "tracknode.h"

using namespace au::au3audio;

namespace {
//! The worker renders in chunks of this duration ...
constexpr double renderChunkDuration = 0.05;
//! ... up to this many ahead: as AudioIO, which buffers 0.25 s in batches of 0.05 s
constexpr size_t renderAheadNumChunks = 5;

using Segments = std::vector<std::pair<int64_t, int64_t> >;

//! The segments that start at `from`, cut at `end`
Segments cut(const Segments& segments, int64_t from, int64_t end)
{
    Segments result;
    for (const auto& [position, length] : segments) {
        if (from >= end) {
            break;
        }
        result.push_back({ position, std::min(length, end - from) });
        from += length;
    }
    return result;
}
}

JuceWorkerPlaybackStream::~JuceWorkerPlaybackStream()
{
    stop();
}

void JuceWorkerPlaybackStream::build(const TransportSequences& sequences, const audio::IAudioMeterPtr& meter,
                               std::optional<double> leadInEndTime)
{
    m_meter = meter;
    m_context.meter = meter;
    m_context.sampleRate = m_sampleRate;
    initializeEffects(sequences.playbackSequences);

    const auto maxFrames = static_cast<size_t>(m_blockSize);
    // Not compensated again if it changes during the stream
    int maxTrackLatency = 0;
    std::vector<std::unique_ptr<TrackNode> > tracks;
    for (size_t i = 0; i < m_sequences.size(); ++i) {
        const auto& sequence = m_sequences[i];
        const auto group = sequence->FindChannelGroup();
        const auto latency = group ? effectsLatency(project, group, m_sampleRate) : 0;
        maxTrackLatency = std::max(maxTrackLatency, latency);
        auto track = std::make_unique<TrackNode>(sequence, sequences.playbackGraphs[i], m_playHead, m_context, latency, m_sampleRate,
                                                 maxFrames);
        // As AudioIO, sequences recorded into with a lead-in play only during the lead-in
        if (leadInEndTime && muse::contains(sequences.leadInTimeSequences, sequence)) {
            track->stopPlayingAt(toSamples(*leadInEndTime));
        }
        tracks.push_back(std::move(track));
    }

    MixNode::Settings settings;
    settings.project = m_wProject;
    settings.sequences = m_sequences;
    settings.masterLatency = effectsLatency(project, RealtimeEffectManager::MasterGroup, m_sampleRate);
    settings.sampleRate = m_sampleRate;
    settings.maxFrames = maxFrames;
    settings.outputLatency = outputLatency();
    m_maxLatency = settings.masterLatency + maxTrackLatency;
    m_mix = std::make_shared<MixNode>(std::move(tracks), std::move(settings), m_context, *m_effectsInitialization, m_playHead,
                                      m_endPosition);

    const auto chunkSize = static_cast<size_t>(std::lround(renderChunkDuration * m_sampleRate));
    // The device waits for the worker: after a seek, it resumes where the device is
    m_queue = std::make_unique<LockFreeQueueNode>(m_mix, numChannels, chunkSize, renderAheadNumChunks, 0);
    for (int ch = 0; ch < numChannels; ++ch) {
        m_buffer[ch].assign(maxFrames, 0.f);
        m_bufferPointers[ch] = m_buffer[ch].data();
    }
}

void JuceWorkerPlaybackStream::setPlayRange(double startTime, double endTime, std::optional<std::pair<double, double> > loopRange)
{
    m_endPosition = toSamples(endTime);
    std::optional<StreamPlayHead::Range> loop;
    if (loopRange) {
        // Unlike AudioIO, it doesn't loop ranges of 50 samples or fewer
        loop = toLoopRange(*loopRange);
    }
    m_playHead.start(toSamples(startTime), loop);
    m_queue->Start();
}

void JuceWorkerPlaybackStream::seek(double time)
{
    m_playHead.setPosition(toSamples(time));
    m_queue->Invalidate();
}

void JuceWorkerPlaybackStream::updateLoop(std::optional<std::pair<double, double> > loopRange, double endTime)
{
    // From where the device is, further than may have been rendered ahead
    const auto from = m_playHead.deviceIndex();
    const auto horizon = from + static_cast<int64_t>(renderAheadNumChunks + 1)
                         * std::lround(renderChunkDuration * m_sampleRate) + m_maxLatency;
    const auto before = segments(from, horizon);
    if (loopRange) {
        m_playHead.setLoop(toLoopRange(*loopRange));
    } else {
        m_endPosition = toSamples(endTime);
        m_playHead.setLoop(std::nullopt);
    }
    // Only if what was rendered ahead changes, which includes what the sequences were read ahead for their effects'
    // latency. Loaded after the change, what was rendered before it is included.
    const auto end = std::min<int64_t>(m_queue->ReadEnd().as_long_long() + m_maxLatency, horizon);
    if (cut(segments(from, horizon), from, end) != cut(before, from, end)) {
        m_queue->Invalidate();
    }
}

Segments JuceWorkerPlaybackStream::segments(int64_t from, int64_t end) const
{
    Segments segments;
    for (auto index = from; index < end;) {
        const auto [position, length] = m_playHead.segment(index, end - index);
        segments.push_back({ position, length });
        index += length;
    }
    return segments;
}

StreamPlayHead::Range JuceWorkerPlaybackStream::toLoopRange(const std::pair<double, double>& loopRange) const
{
    const auto start = toSamples(loopRange.first);
    return { start, std::max(start + 1, toSamples(loopRange.second)) };
}

int JuceWorkerPlaybackStream::renderChunk(float* const* output, int numOutputChannels, int offset, int requested,
                                    Clock::time_point)
{
    const auto index = m_playHead.deviceIndex();
    m_queue->Prepare(index);
    // As AudioIO: what the worker hasn't rendered yet is played late, after silence
    const auto frames = static_cast<int>(m_queue->Available(index, static_cast<size_t>(requested)));
    m_unplayedFrames += requested - frames;
    m_queue->GetSamples(m_bufferPointers, index, static_cast<size_t>(frames));

    // What plays now: unless looping, the end of the play range ends it
    bool finished = false;
    if (m_playHead.loop()) {
        for (int64_t done = 0; done < frames;) {
            const auto [position, length] = m_playHead.segment(index + done, frames - done);
            reportPlayed(position, length);
            done += length;
        }
    } else {
        const auto start = m_playHead.linearPosition(index);
        const auto inRange = std::clamp<int64_t>(m_endPosition.load() - start, 0, frames);
        if (inRange > 0) {
            reportPlayed(start, inRange);
        }
        if (inRange < frames) {
            for (auto& buffer : m_buffer) {
                std::fill(buffer.begin() + inRange, buffer.begin() + frames, 0.f);
            }
            finished = true;
        }
    }

    const float* stereo[numChannels] { m_bufferPointers[0], m_bufferPointers[1] };
    writeOutput(stereo, output, numOutputChannels, offset, frames);
    m_playHead.advance(frames);
    if (finished) {
        finish();
    }
    return requested - frames;
}

std::string JuceWorkerPlaybackStream::dropoutDetails() const
{
    return ", " + muse::String::number(1000.0 * m_unplayedFrames / m_sampleRate, 0).toStdString()
           + " ms of silence waiting for rendering";
}

void JuceWorkerPlaybackStream::releaseRendering()
{
    // Stops the worker
    m_queue.reset();
    m_mix.reset();
}
