/*
* Audacity: A Digital Audio Editor
*/
#include "mixnode.h"

#include <algorithm>

#include "sequencerenderer.h"
#include "streamplayhead.h"
#include "tracknode.h"

using namespace au::au3audio;

MixNode::MixNode(std::vector<std::unique_ptr<TrackNode> > tracks, Settings settings, RenderContext& context,
                 RealtimeEffects::InitializationScope& effects, const StreamPlayHead& playHead,
                 const std::atomic<int64_t>& endPosition)
    : m_tracks{std::move(tracks)}, m_settings{std::move(settings)}, m_context{context}, m_effects{effects}, m_playHead{playHead},
    m_endPosition{endPosition}
{
    m_scratch.allocate(static_cast<int>(m_settings.maxFrames));
    for (int ch = 0; ch < numChannels; ++ch) {
        m_track[ch].assign(m_settings.maxFrames, 0.f);
        m_trackPointers[ch] = m_track[ch].data();
        m_discarded[ch].assign(m_settings.maxFrames, 0.f);
        m_discardedPointers[ch] = m_discarded[ch].data();
    }
    m_interleaved.resize(m_settings.maxFrames * numChannels);
}

MixNode::~MixNode() = default;

void MixNode::GetSamples(float* const* buffers, sampleCount start, size_t len)
{
    RealtimeEffects::ProcessingScope effects { m_effects, m_settings.project };
    m_context.effects = &effects;
    m_context.hasSoloingSequence = std::any_of(m_settings.sequences.begin(), m_settings.sequences.end(),
                                               [](const auto& sequence) { return sequence->GetSolo(); });
    // Unless looping, the sequences go silent at the end
    if (m_playHead.loop()) {
        m_context.endPosition.reset();
    } else {
        m_context.endPosition = m_endPosition.load();
    }

    const auto index = start.as_long_long();
    for (size_t done = 0; done < len;) {
        const auto frames = std::min(len - done, m_settings.maxFrames);
        float* output[numChannels] { buffers[0] + done, buffers[1] + done };
        render(output, index + static_cast<int64_t>(done), frames);
        done += frames;
    }
    m_context.effects = nullptr;
}

void MixNode::render(float* const* output, int64_t index, size_t frames)
{
    // When it will be heard, as the device goes now
    m_context.dacTime = Clock::now() + toDuration(index - m_playHead.deviceIndex(), m_settings.sampleRate)
                        + m_settings.outputLatency;

    const auto latency = m_settings.masterLatency;
    if (m_next != index) {
        // What the master effects output for the samples before `index` reaches here, delayed by their latency
        for (int64_t done = 0; done < latency;) {
            const auto count = static_cast<size_t>(std::min<int64_t>(latency - done, static_cast<int64_t>(m_settings.maxFrames)));
            mix(m_discardedPointers, index + done, count);
            renderMaster(m_context, m_scratch, m_discardedPointers, count);
            done += static_cast<int64_t>(count);
        }
    }
    mix(output, index + latency, frames);
    renderMaster(m_context, m_scratch, output, frames);
    m_next = index + static_cast<int64_t>(frames);

    sendToMeters(output, frames);
}

void MixNode::mix(float* const* output, int64_t index, size_t frames)
{
    for (int ch = 0; ch < numChannels; ++ch) {
        std::fill_n(output[ch], frames, 0.f);
    }
    for (const auto& track : m_tracks) {
        track->GetSamples(m_trackPointers, index, frames);
        for (int ch = 0; ch < numChannels; ++ch) {
            for (size_t i = 0; i < frames; ++i) {
                output[ch][i] += m_track[ch][i];
            }
        }
    }
}

void MixNode::sendToMeters(float* const* output, size_t frames)
{
    const auto& meter = m_context.meter;
    if (!meter) {
        return;
    }
    // From this thread only: the meter takes one producer
    for (const auto& track : m_tracks) {
        track->sendToMeter();
    }
    // The meter shows the mix before the playback volume
    for (size_t i = 0; i < frames; ++i) {
        for (int ch = 0; ch < numChannels; ++ch) {
            m_interleaved[i * numChannels + ch] = output[ch][i];
        }
    }
    for (int ch = 0; ch < numChannels; ++ch) {
        meter->push(ch, { m_interleaved.data() + ch, frames, numChannels, m_context.dacTime });
    }
}
