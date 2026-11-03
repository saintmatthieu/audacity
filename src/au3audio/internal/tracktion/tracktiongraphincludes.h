/*
* Audacity: A Digital Audio Editor
*/
#pragma once

// Qt's keyword macros clash with identifiers in tracktion_graph; the includers don't use them
#undef slots
#undef signals
#undef emit
#include <juce_audio_devices/juce_audio_devices.h>
#include <tracktion_graph/tracktion_graph.h>
