/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <cstdint>

#include "juceplaybackstream.h"

namespace au::au4juce {
//! Plays the mix of the sequences, rendered in step with the device, as the Tracktion backend does, without its pool
/*!
 * A worker per sequence reads it ahead, as the play head says, and the device thread renders the effects, the mix and
 * the master in step with each block. Edits are heard within a block; a seek or a change of what loops is heard after
 * ~0.1 s of silence, until the workers resynchronize. What isn't rendered in time is skipped.
 */
class JuceDevicePlaybackStream final : public JucePlaybackStream
{
public:
    using JucePlaybackStream::JucePlaybackStream;
    ~JuceDevicePlaybackStream() override;

private:
    const char* name() const override { return "JUCE in step"; }

    std::optional<TrackNode::ReadAhead> trackReadAhead() const override;
    void startReadingAhead() override;
    void invalidateReadAhead() override;
    int64_t readAheadEnd() const override;
    size_t render(float* const* stereo, int64_t index, size_t frames) override;
};
}
