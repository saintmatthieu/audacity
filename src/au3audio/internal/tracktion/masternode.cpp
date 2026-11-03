/*
* Audacity: A Digital Audio Editor
*/
#include "masternode.h"

#include "au3-realtime-effects/RealtimeEffectManager.h"

using namespace au::au3audio;

MasterNode::MasterNode(std::unique_ptr<tracktion::graph::Node> input, const RenderContext& context, int latency, size_t nodeId)
    : m_input{std::move(input)}, m_context{context}, m_latency{latency}, m_nodeId{nodeId}
{
}

tracktion::graph::NodeProperties MasterNode::getNodeProperties()
{
    tracktion::graph::NodeProperties props;
    props.hasAudio = true;
    props.numberOfChannels = numChannels;
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
    choc::buffer::copy(pc.buffers.audio, m_input->getProcessedOutput().audio);
    if (m_context.effects) {
        float* buffers[numChannels] { channelPointer(pc.buffers.audio, 0), channelPointer(pc.buffers.audio, 1) };
        // As for the sequences, the output stays delayed by m_latency
        m_context.effects->Process(RealtimeEffectManager::MasterGroup, buffers, m_scratch.scratch(), m_scratch.dummy(),
                                   numChannels, pc.numSamples);
    }
}
