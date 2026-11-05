/*
* Audacity: A Digital Audio Editor
*/
#include "playbackstream.h"

#include <algorithm>
#include <chrono>

#include "framework/global/log.h"

#include "au3-project/Project.h"
#include "au3-track/Track.h"

#include "recordingcapture.h"

using namespace au::au4juce;

PlaybackStream::PlaybackStream(AudacityProject& project, int token, std::unique_ptr<juce::AudioIODevice> device,
                               bool followsPlayRegion, LockFreeQueue<audio::AudioCallbackInfo>& callbackInfos,
                               LockFreeQueue<PlayedRange>& playedRanges, const std::atomic<float>& playbackVolume)
    : project{project}, token{token}, followsPlayRegion{followsPlayRegion}, m_wProject{project.shared_from_this()},
    m_sampleRate{device->getCurrentSampleRate()}, m_blockSize{std::max(1, device->getCurrentBufferSizeSamples())},
    m_callbackInfos{callbackInfos}, m_playedRanges{playedRanges}, m_playbackVolume{playbackVolume}, m_device{std::move(device)}
{
}

PlaybackStream::~PlaybackStream() = default;

void PlaybackStream::initializeEffects(const ConstPlayableSequences& sequences)
{
    m_sequences = sequences;
    // At the rate of the stream, not of the sequences
    m_effectsInitialization.emplace(m_wProject, m_sampleRate, numChannels, m_blockSize);
    for (const auto& sequence : m_sequences) {
        if (const auto group = sequence->FindChannelGroup()) {
            m_effectsInitialization->AddGroup(*group, numChannels, m_sampleRate, m_blockSize);
        }
    }
}

Clock::duration PlaybackStream::outputLatency() const
{
    return toDuration(m_device->getOutputLatencyInSamples(), m_sampleRate);
}

void PlaybackStream::start()
{
    if (m_meter) {
        m_meter->start(m_sampleRate);
    }
    m_device->start(this);
}

void PlaybackStream::setCapture(std::unique_ptr<RecordingCapture> capture)
{
    m_capture = std::move(capture);
}

void PlaybackStream::stop()
{
    if (m_device) {
        m_device->stop();
        // Underruns of the output, and overflows of the input if one is open
        const int xruns = m_device->getXRunCount();
        LOGI() << "Playback dropouts (" << name() << "): " << muse::String::number(m_dropouts.frames / m_sampleRate, 1)
               << " s played, " << xruns << " device underruns" << (m_capture ? " or input overflows" : "") << ", "
               << m_dropouts.lateCallbacks << " late callbacks (longest " << muse::String::number(1000.0 * m_dropouts.longestCallback, 1)
               << " ms for " << muse::String::number(1000.0 * m_dropouts.longestCallbackBlock, 1) << " ms of audio)"
               << dropoutDetails();
        m_device->close();
        m_device.reset();
    }
    // Once the device stopped, so that what it captured last is recorded
    if (m_capture) {
        m_capture->finish();
    }
    if (m_meter) {
        m_meter->stop();
    }
    releaseRendering();
    // After the device stopped, so that the callback can't use them
    m_effectsInitialization.reset();
}

RealtimeEffects::InitializationScope* PlaybackStream::effectsInitialization()
{
    return m_effectsInitialization ? &*m_effectsInitialization : nullptr;
}

void PlaybackStream::audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                                                      float* const* outputChannelData, int numOutputChannels, int numSamples,
                                                      const juce::AudioIODeviceCallbackContext&)
{
    for (int ch = 0; ch < numOutputChannels; ++ch) {
        if (outputChannelData[ch]) {
            std::fill_n(outputChannelData[ch], numSamples, 0.f);
        }
    }

    if (m_finished || !m_effectsInitialization) {
        return;
    }
    const bool paused = m_paused;
    if (paused && !m_capture) {
        return;
    }

    const auto callbackStart = Clock::now();
    const auto dacTime = callbackStart + outputLatency();
    // The input of the channels the capture records, if the device gives them
    const auto input = m_capture && numInputChannels >= m_capture->numInputChannels() ? inputChannelData : nullptr;

    int done = 0;
    int unplayed = 0;
    while (done < numSamples && !m_finished) {
        const int frames = std::min(numSamples - done, m_blockSize);
        const auto chunkDacTime = dacTime + toDuration(done, m_sampleRate);
        if (!paused) {
            unplayed += renderChunk(outputChannelData, numOutputChannels, done, frames, chunkDacTime);
        }
        if (input) {
            m_capture->process(input, outputChannelData, numOutputChannels, done, frames, position(), chunkDacTime, paused);
        }
        done += frames;
    }

    if (!paused) {
        // As AudioIO, only what played advances the transport
        if (numSamples > unplayed) {
            m_callbackInfos.Put({ dacTime, numSamples - unplayed });
        }

        const double duration = std::chrono::duration<double>(Clock::now() - callbackStart).count();
        const double block = numSamples / m_sampleRate;
        m_dropouts.frames += numSamples;
        if (duration > block) {
            ++m_dropouts.lateCallbacks;
        }
        if (duration > m_dropouts.longestCallback) {
            m_dropouts.longestCallback = duration;
            m_dropouts.longestCallbackBlock = block;
        }
    }
}

void PlaybackStream::audioDeviceError(const juce::String& errorMessage)
{
    LOGE() << "audio device error: " << errorMessage.toStdString();
}

void PlaybackStream::reportPlayed(int64_t start, int64_t frames)
{
    m_playedRanges.Put({ toTime(start), static_cast<unsigned long>(frames) });
}

void PlaybackStream::sendToMainMeter(const float* const* stereo, int frames, Clock::time_point dacTime)
{
    if (!m_meter) {
        return;
    }
    m_interleaved.resize(static_cast<size_t>(frames) * numChannels);
    for (int i = 0; i < frames; ++i) {
        for (int ch = 0; ch < numChannels; ++ch) {
            m_interleaved[i * numChannels + ch] = stereo[ch][i];
        }
    }
    for (int ch = 0; ch < numChannels; ++ch) {
        m_meter->push(ch, { m_interleaved.data() + ch, static_cast<size_t>(frames), numChannels, dacTime });
    }
}

void PlaybackStream::writeOutput(const float* const* stereo, float* const* output, int numOutputChannels, int offset, int frames) const
{
    const float volume = m_playbackVolume.load(std::memory_order_relaxed);
    for (int ch = 0; ch < numOutputChannels; ++ch) {
        if (!output[ch]) {
            continue;
        }
        for (int i = 0; i < frames; ++i) {
            const float sample = numOutputChannels == 1
                                 ? 0.5f * (stereo[0][i] + stereo[1][i])
                                 : ch < numChannels ? stereo[ch][i] : 0.f;
            output[ch][offset + i] = sample * volume;
        }
    }
}
