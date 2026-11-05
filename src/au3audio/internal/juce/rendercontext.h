/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

#include "audio/iaudiometer.h"

namespace RealtimeEffects {
class ProcessingScope;
}

namespace au::au3audio {
//! The graph and the device are stereo
constexpr int numChannels = 2;

using Clock = std::chrono::steady_clock;

//! Per-callback state shared by the nodes; written by the device callback before processing the graph
struct RenderContext {
    RealtimeEffects::ProcessingScope* effects = nullptr;
    Clock::time_point dacTime;
    bool hasSoloingSequence = false;
    //! Where the sequences go silent, unless looping
    std::optional<int64_t> endPosition;
    //! Constant during a stream
    audio::IAudioMeterPtr meter;
    //! Of the whole graph, in samples
    int latency = 0;
    //! Constant during a stream
    double sampleRate = 0.0;
};

inline Clock::duration toDuration(int64_t samples, double sampleRate)
{
    return std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(samples / sampleRate));
}
}
