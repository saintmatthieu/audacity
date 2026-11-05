/*
* Audacity: A Digital Audio Editor
*/
#include "sequencerenderer.h"

#include <algorithm>

#include "au3-mixer/AudioIOSequences.h"
#include "au3-realtime-effects/RealtimeEffectManager.h"
#include "au3-wave-track/WaveTrack.h"

using namespace au::au4juce;

SequenceRenderer::SequenceRenderer(std::shared_ptr<const PlayableSequence> sequence, const RenderContext& context, int latency)
    : m_sequence{std::move(sequence)}, m_context{context}, m_latency{latency}
{
    const auto waveTrack = dynamic_cast<const WaveTrack*>(m_sequence->FindChannelGroup());
    m_trackId = waveTrack ? static_cast<int64_t>(waveTrack->GetId()) : -1;
}

void SequenceRenderer::allocate(size_t maxFrames)
{
    for (int ch = 0; ch < numChannels; ++ch) {
        m_work[ch].assign(maxFrames, 0.f);
        m_input[ch] = m_work[ch].data();
        m_meterBuffers[ch].assign(maxFrames, 0.f);
    }
    m_scratch.allocate(static_cast<int>(maxFrames));
}

void SequenceRenderer::render(float* const* output, size_t frames, int64_t linearStart)
{
    const auto sequenceChannels = std::min<size_t>(m_sequence->NChannels(), numChannels);
    for (size_t ch = sequenceChannels; ch < numChannels; ++ch) {
        // Effects may crash on null buffers, even for channels they don't use
        std::fill_n(m_work[ch].data(), frames, 0.f);
    }

    // Past the end of the play range, only what is still in the effects' latency is heard; past where the sequence stops,
    // the same
    auto end = m_context.endPosition;
    if (m_stopPosition) {
        end = std::min(end.value_or(*m_stopPosition), *m_stopPosition);
    }
    if (end) {
        const auto audible = static_cast<size_t>(std::clamp<int64_t>(*end - linearStart, 0, frames));
        for (auto& buffer : m_work) {
            std::fill(buffer.data() + audible, buffer.data() + frames, 0.f);
        }
    }

    if (const auto group = m_sequence->FindChannelGroup(); m_context.effects && group) {
        // Ignores how many samples to discard for the effects' latency: the output stays delayed by m_latency, which the
        // caller compensates
        m_context.effects->Process(group, m_input, m_scratch.scratch(), m_scratch.dummy(), numChannels, frames);
    }

    const bool silenced = !m_sequence->GetSolo() && (m_context.hasSoloingSequence || m_sequence->GetMute());
    float maxVolume = 0.f;
    for (int ch = 0; ch < numChannels; ++ch) {
        const float volume = silenced ? 0.f : m_sequence->GetChannelVolume(ch);
        maxVolume = std::max(maxVolume, volume);
        const auto* const src = m_work[sequenceChannels > 1 ? ch : 0].data();
        auto* const dst = output[ch];
        for (size_t i = 0; i < frames; ++i) {
            dst[i] = src[i] * volume;
        }
    }

    if (m_context.meter) {
        // This output is heard after the latency it is delayed by downstream
        m_meterTime = m_context.dacTime + toDuration(m_context.latency - m_latency, m_context.sampleRate);
        m_meterFrames = frames;
        if (sequenceChannels > 1) {
            m_meterChannels = numChannels;
            for (int ch = 0; ch < numChannels; ++ch) {
                std::copy_n(output[ch], frames, m_meterBuffers[ch].data());
            }
        } else {
            m_meterChannels = 1;
            for (size_t i = 0; i < frames; ++i) {
                m_meterBuffers[0][i] = m_work[0][i] * maxVolume;
            }
        }
    }
}

void SequenceRenderer::sendToMeter()
{
    const auto& meter = m_context.meter;
    if (!meter || m_meterFrames == 0) {
        return;
    }
    const auto trackId = audio::IAudioMeter::TrackId { m_trackId };
    for (size_t ch = 0; ch < m_meterChannels; ++ch) {
        meter->push(static_cast<uint8_t>(ch), { m_meterBuffers[ch].data(), m_meterFrames, 1, m_meterTime }, trackId);
    }
    m_meterFrames = 0;
}

void au::au4juce::renderMaster(const RenderContext& context, EffectScratch& scratch, float* const* stereo, size_t frames)
{
    if (context.effects) {
        // As for the sequences, the output stays delayed by the effects' latency
        context.effects->Process(RealtimeEffectManager::MasterGroup, stereo, scratch.scratch(), scratch.dummy(), numChannels, frames);
    }
}
