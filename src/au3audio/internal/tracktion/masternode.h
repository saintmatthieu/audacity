/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <memory>
#include <vector>

#include "../juce/effectscratch.h"
#include "../juce/rendercontext.h"
#include "tracktiongraphincludes.h"

namespace au::au4tracktion {
//! Applies the master realtime effects to the mix of all sequences
class MasterNode final : public tracktion::graph::Node
{
public:
    MasterNode(std::unique_ptr<tracktion::graph::Node> input, const au4juce::RenderContext& context, int latency, size_t nodeId);

    tracktion::graph::NodeProperties getNodeProperties() override;
    std::vector<tracktion::graph::Node*> getDirectInputNodes() override { return { m_input.get() }; }
    bool isReadyToProcess() override { return m_input->hasProcessed(); }
    void prepareToPlay(const tracktion::graph::PlaybackInitialisationInfo& info) override;
    void process(tracktion::graph::Node::ProcessContext& pc) override;

private:
    const std::unique_ptr<tracktion::graph::Node> m_input;
    const au4juce::RenderContext& m_context;
    const int m_latency;
    const size_t m_nodeId;
    au4juce::EffectScratch m_scratch;
};
}
