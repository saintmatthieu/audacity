/**********************************************************************

  Audacity: A Digital Audio Editor

  @file ClapInstance.h

  @brief PerTrackEffect::Instance that drives a CLAP plugin via ClapWrapper.

  This header stays free of the CLAP SDK so it can be included by the AU4
  effects module; ClapWrapper is referenced only by forward declaration.

**********************************************************************/
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "au3-effects/PerTrackEffect.h"

class ClapWrapper;
class ClapEntry;

class CLAP_API ClapInstance final : public PerTrackEffect::Instance
{
public:
    ClapInstance(const PerTrackEffect& effect, std::shared_ptr<ClapEntry> entry, std::string pluginId);
    ~ClapInstance() override;

    ClapInstance(const ClapInstance&) = delete;
    ClapInstance& operator=(const ClapInstance&) = delete;

    // EffectInstance
    size_t GetBlockSize() const override;
    size_t SetBlockSize(size_t maxBlockSize) override;
    unsigned GetAudioInCount() const override;
    unsigned GetAudioOutCount() const override;

    // Realtime processing
    bool RealtimeInitialize(EffectSettings& settings, double sampleRate, size_t audioThreadBufferSize) override;
    bool RealtimeAddProcessor(EffectSettings& settings, EffectOutputs* pOutputs, unsigned numChannels, float sampleRate) override;
    bool RealtimeFinalize(EffectSettings& settings) noexcept override;
    bool RealtimeSuspend() override;
    bool RealtimeResume() override;
    bool RealtimeProcessStart(MessagePackage& package) override;
    size_t RealtimeProcess(size_t group, EffectSettings& settings, const float* const* inBuf, float* const* outBuf,
                           size_t numSamples) override;
    bool RealtimeProcessEnd(EffectSettings& settings) noexcept override;

    SampleCount GetLatency(const EffectSettings& settings, double sampleRate) const override;

    // Destructive processing
    bool ProcessInitialize(EffectSettings& settings, double sampleRate, ChannelNames chanMap) override;
    bool ProcessFinalize() noexcept override;
    size_t ProcessBlock(EffectSettings& settings, const float* const* inBlock, float* const* outBlock, size_t blockLen) override;

    ClapWrapper& GetWrapper();

private:
    std::shared_ptr<ClapEntry> mEntry;
    std::string mPluginId;
    std::unique_ptr<ClapWrapper> mWrapper;

    size_t mBlockSize { 8192 };
    sampleCount mInitialDelay { 0 };

    // Realtime: this instance is processor 0; extra channel groups get their own.
    bool mRecruited { false };
    std::vector<std::unique_ptr<ClapInstance> > mProcessors;
    size_t m_audioThreadBufferSize { 0 };
};
