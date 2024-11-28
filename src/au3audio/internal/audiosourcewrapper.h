#pragma once

#include "libraries/lib-channel/Channel.h"
#include "libraries/lib-utility/Observer.h"
#include "iaudiosource.h"
#include "audioenginetypes.h"
#include "async/channel.h"

namespace au::audio {
class AudioSourceWrapper : public ChannelGroup, public std::enable_shared_from_this<AudioSourceWrapper>
{
public:
    AudioSourceWrapper(IAudioSource* source, muse::async::Channel<const IAudioSource*, EffectChainLinkIndex, EffectChainLinkPtr> realtimeEffectAppended);

    const IAudioSource* source() const { return m_source; }

    void ShiftBy(double t0, double delta) override;
    void MoveTo(double o) override;
    size_t NChannels() const override;
    size_t NIntervals() const override;
    std::shared_ptr<Channel> DoGetChannel(size_t iChannel) override;
    std::shared_ptr<Interval> DoGetInterval(size_t iInterval) override;

private:
    const IAudioSource* const m_source;
    const Observer::Subscription m_realtimeEffectListSubscription;
    muse::async::Channel<const IAudioSource*, EffectChainLinkIndex, EffectChainLinkPtr> m_realtimeEffectAppended;
};
}
