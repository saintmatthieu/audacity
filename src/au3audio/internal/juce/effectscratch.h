/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <vector>

#include "rendercontext.h"

namespace au::au4juce {
//! Scratch space in the layout RealtimeEffectManager::Process() expects
class EffectScratch
{
public:
    void allocate(int frames);

    float* const* scratch() { return m_scratch; }
    float* dummy() { return m_buffers[numChannels].data(); }

private:
    std::vector<float> m_buffers[numChannels + 1];
    float* m_scratch[numChannels] {};
};
}
