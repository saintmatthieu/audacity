/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <cstdint>
#include <utility>

// Qt's keyword macros clash with identifiers in tracktion_graph; the includers don't use them
#undef slots
#undef signals
#undef emit
#include <juce_audio_devices/juce_audio_devices.h>
#include <tracktion_graph/tracktion_graph.h>

namespace au::au3audio {
//! Where the play head maps a timeline position it plays linearly, and how far it plays on from there before looping, at
//! most `maxLen`
std::pair<int64_t, int64_t> playHeadSegment(const tracktion::graph::PlayHead& playHead, int64_t linearPosition,
                                            int64_t maxLen);
}
