/*
* Audacity: A Digital Audio Editor
*/
#include "masternode.h"

#include "../juce/sequencerenderer.h"

using namespace au::au4tracktion;

MasterNode::MasterNode(std::unique_ptr<tracktion::graph::Node> input, const au4juce::RenderContext& context, int latency, size_t nodeId)
    : m_input{std::move(input)}, m_context{context}, m_latency{latency}, m_nodeId{nodeId}
{
}

tracktion::graph::NodeProperties MasterNode::getNodeProperties()
{
    tracktion::graph::NodeProperties props;
    props.hasAudio = true;
    props.numberOfChannels = au4juce::numChannels;
    props.latencyNumSamples = m_input->getNodeProperties().latencyNumSamples + m_latency;
    props.nodeID = m_nodeId;
    return props;
}

void MasterNode::prepareToPlay(const tracktion::graph::PlaybackInitialisationInfo& info)
{
    m_scratch.allocate(info.blockSize);
}

void MasterNode::process(tracktion::graph::Node::ProcessContext& pc)
{
    const auto input = m_input->getProcessedOutput().audio;
    if (input.getNumChannels() == pc.buffers.audio.getNumChannels()) {
        choc::buffer::copy(pc.buffers.audio, input);
    } else {
        // Nothing plays, e.g. while recording the first track
        pc.buffers.audio.clear();
    }
    float* buffers[au4juce::numChannels] { channelPointer(pc.buffers.audio, 0), channelPointer(pc.buffers.audio, 1) };
    // As for the sequences, the output stays delayed by m_latency
    au4juce::renderMaster(m_context, m_scratch, buffers, pc.numSamples);
}
