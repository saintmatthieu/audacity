#include "audiosourcewrapper.h"
#include "libraries/lib-realtime-effects/RealtimeEffectList.h"
#include "log.h"
#include <cassert>

namespace au::audio {
class AudioSourceChannel : public ::Channel
{
public:
    AudioSourceChannel(AudioSourceWrapper& source, int channel)
        : m_source{source}, m_channel{channel}
    {
    }

private:
    ChannelGroup& DoGetChannelGroup() const override { return m_source; }

    AudioSourceWrapper& m_source;
    const int m_channel;
};

AudioSourceWrapper::AudioSourceWrapper(IAudioSource* source,
                                       muse::async::Channel<const IAudioSource*, EffectChainLinkIndex,
                                                            EffectChainLinkPtr> realtimeEffectAppended)
    : m_source{source},
    m_realtimeEffectListSubscription{RealtimeEffectList::Get(*this).Subscribe([this](const RealtimeEffectListMessage& msg)
    {
        switch (msg.type) {
            case RealtimeEffectListMessage::Type::Insert:
            {
                auto newEffect = std::make_shared<EffectChainLink>("Effect name", msg.affectedState.get());
                m_realtimeEffectAppended.send(m_source, msg.srcIndex, std::move(newEffect));
            }
            break;
                // case RealtimeEffectListMessage::Type::Remove:
                // {
                //     auto oldEffect = std::make_shared<EffectChainLink>(getEffectName(*msg.affectedState), msg.affectedState.get());
                //     m_realtimeEffectRemoved.send(trackId, msg.srcIndex, std::move(oldEffect));
                // }
                // break;
                // case RealtimeEffectListMessage::Type::DidReplace:
                // {
                //     const std::shared_ptr<RealtimeEffectState> newState = list.GetStateAt(msg.dstIndex);
                //     IF_ASSERT_FAILED(newState) {
                //         return;
                //     }
                //     auto oldEffect = std::make_shared<EffectChainLink>(getEffectName(*msg.affectedState), msg.affectedState.get());
                //     auto newEffect = std::make_shared<EffectChainLink>(getEffectName(*newState), newState.get());
                //     m_realtimeEffectReplaced.send(trackId, msg.srcIndex, std::move(oldEffect), std::move(newEffect));
                // }
                // break;
        }
    })},
    m_realtimeEffectAppended{realtimeEffectAppended}
{
}

void AudioSourceWrapper::ShiftBy(double t0, double delta)
{
    UNUSED(t0);
    UNUSED(delta);
    // Not needed by audio engine
    assert(false);
}

void AudioSourceWrapper::MoveTo(double o)
{
    UNUSED(o);
    // Not needed by audio engine (I think)
    assert(false);
}

size_t AudioSourceWrapper::NChannels() const
{
    return m_source->numChannels();
}

size_t AudioSourceWrapper::NIntervals() const
{
    // Not needed by audio engine
    assert(false);
    return 1;
}

std::shared_ptr<Channel> AudioSourceWrapper::DoGetChannel(size_t iChannel)
{
    return std::make_shared<AudioSourceChannel>(*this, iChannel);
}

std::shared_ptr<::ChannelGroup::Interval> AudioSourceWrapper::DoGetInterval(size_t iInterval)
{
    UNUSED(iInterval);
    // Not needed by audio engine
    assert(false);
    return nullptr;
}
}
