/*
* Audacity: A Digital Audio Editor
*/
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "effectscratch.h"
#include "rendercontext.h"
#include "tracktiongraphincludes.h"

class AudacityProject;
class ISequenceProcessingNode;
class Mixer;
class PlayableSequence;

namespace au::au3audio {
//! Renders one playback sequence: pulls its samples from a Mixer (which resamples and stretches),
//! applies its realtime effects, mute/solo and volume/pan, and feeds its meter.
class SequenceNode final : public tracktion::graph::Node
{
public:
    //! @param graph the graph processing `sequence` as it is played, where its samples are read
    SequenceNode(std::shared_ptr<const PlayableSequence> sequence, std::shared_ptr<ISequenceProcessingNode> graph,
                 const tracktion::graph::PlayHead& playHead, const RenderContext& context, const AudacityProject& project,
                 double mixerStartTime, double mixerEndTime, int latency, size_t nodeId);
    ~SequenceNode() override;

    tracktion::graph::NodeProperties getNodeProperties() override;
    bool isReadyToProcess() override { return true; }
    void prepareToPlay(const tracktion::graph::PlaybackInitialisationInfo& info) override;
    void process(tracktion::graph::Node::ProcessContext& pc) override;

private:
    const std::shared_ptr<const PlayableSequence> m_sequence;
    const std::shared_ptr<ISequenceProcessingNode> m_graph;
    const tracktion::graph::PlayHead& m_playHead;
    const RenderContext& m_context;
    const AudacityProject& m_project;
    const double m_mixerStartTime;
    const double m_mixerEndTime;
    const int m_latency;
    const size_t m_nodeId;
    int64_t m_trackId = -1;

    double m_sampleRate = 0.0;
    std::unique_ptr<Mixer> m_mixer;
    std::optional<int64_t> m_nextTimelinePosition;
    std::vector<float> m_work[numChannels];
    std::vector<float> m_meterBuffer;
    EffectScratch m_scratch;
};
}
