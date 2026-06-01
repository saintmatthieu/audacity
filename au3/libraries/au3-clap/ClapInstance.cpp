/**********************************************************************

  Audacity: A Digital Audio Editor

  @file ClapInstance.cpp

**********************************************************************/
#include "ClapInstance.h"

#include <algorithm>

#include "au3-exceptions/AudacityException.h"

#include "ClapEntry.h"
#include "ClapWrapper.h"

namespace {
constexpr size_t kMaxBlockSize = 8192;
}

ClapInstance::ClapInstance(const PerTrackEffect& effect, std::shared_ptr<ClapEntry> entry, std::string pluginId)
    : Instance(effect), mEntry(std::move(entry)), mPluginId(std::move(pluginId))
{
    mWrapper = std::make_unique<ClapWrapper>(mEntry, mPluginId);
    mWrapper->InitializeComponents();
}

ClapInstance::~ClapInstance() = default;

size_t ClapInstance::GetBlockSize() const
{
    return mBlockSize;
}

size_t ClapInstance::SetBlockSize(size_t maxBlockSize)
{
    mBlockSize = std::min(maxBlockSize, kMaxBlockSize);
    return mBlockSize;
}

unsigned ClapInstance::GetAudioInCount() const
{
    return mWrapper->GetAudioInCount();
}

unsigned ClapInstance::GetAudioOutCount() const
{
    return mWrapper->GetAudioOutCount();
}

bool ClapInstance::ProcessInitialize(EffectSettings& settings, double sampleRate, ChannelNames)
{
    if (mWrapper->Initialize(settings, sampleRate, mBlockSize)) {
        mInitialDelay = mWrapper->GetLatencySamples();
        return true;
    }
    return false;
}

bool ClapInstance::ProcessFinalize() noexcept
{
    return GuardedCall<bool>([&]{
        mWrapper->Finalize(nullptr);
        return true;
    });
}

size_t ClapInstance::ProcessBlock(EffectSettings&, const float* const* inBlock, float* const* outBlock, size_t blockLen)
{
    return mWrapper->Process(inBlock, outBlock, blockLen);
}

bool ClapInstance::RealtimeInitialize(EffectSettings& settings, double sampleRate, size_t audioThreadBufferSize)
{
    if (mWrapper->Initialize(settings, sampleRate, mBlockSize)) {
        mInitialDelay = mWrapper->GetLatencySamples();
        m_audioThreadBufferSize = audioThreadBufferSize;
        return true;
    }
    return false;
}

bool ClapInstance::RealtimeAddProcessor(EffectSettings& settings, EffectOutputs*, unsigned, float sampleRate)
{
    if (!mRecruited) {
        // This instance serves the first processor group.
        mRecruited = true;
        return true;
    }
    // Additional channel groups need independent plugin instances/state.
    auto& effect = static_cast<const PerTrackEffect&>(mProcessor);
    auto uProcessor = std::make_unique<ClapInstance>(effect, mEntry, mPluginId);
    if (!uProcessor->RealtimeInitialize(settings, sampleRate, m_audioThreadBufferSize)) {
        return false;
    }
    mProcessors.push_back(std::move(uProcessor));
    return true;
}

bool ClapInstance::RealtimeFinalize(EffectSettings& settings) noexcept
{
    return GuardedCall<bool>([&]{
        mRecruited = false;
        mWrapper->Finalize(&settings);
        for (auto& pProcessor : mProcessors) {
            pProcessor->mWrapper->Finalize(nullptr);
        }
        mProcessors.clear();
        return true;
    });
}

bool ClapInstance::RealtimeSuspend()
{
    mWrapper->SuspendProcessing();
    for (auto& pProcessor : mProcessors) {
        pProcessor->mWrapper->SuspendProcessing();
    }
    return true;
}

bool ClapInstance::RealtimeResume()
{
    mWrapper->ResumeProcessing();
    for (auto& pProcessor : mProcessors) {
        pProcessor->mWrapper->ResumeProcessing();
    }
    return true;
}

bool ClapInstance::RealtimeProcessStart(MessagePackage& package)
{
    mWrapper->ProcessBlockStart(package.settings);
    for (auto& pProcessor : mProcessors) {
        pProcessor->mWrapper->ProcessBlockStart(package.settings);
    }
    return true;
}

size_t ClapInstance::RealtimeProcess(size_t group, EffectSettings&, const float* const* inBuf, float* const* outBuf,
                                     size_t numSamples)
{
    if (!mRecruited) {
        return 0;
    }
    if (group == 0) {
        return mWrapper->Process(inBuf, outBuf, numSamples);
    } else if (--group < mProcessors.size()) {
        return mProcessors[group]->mWrapper->Process(inBuf, outBuf, numSamples);
    }
    return 0;
}

bool ClapInstance::RealtimeProcessEnd(EffectSettings&) noexcept
{
    return true;
}

auto ClapInstance::GetLatency(const EffectSettings&, double) const -> SampleCount
{
    return mInitialDelay.as_long_long();
}

ClapWrapper& ClapInstance::GetWrapper()
{
    return *mWrapper;
}

// GUI facade --------------------------------------------------------------------

void ClapInstance::setHostListener(IClapHostListener* listener) { mWrapper->setHostListener(listener); }
IClapHostListener* ClapInstance::hostListener() const { return mWrapper->hostListener(); }
bool ClapInstance::hasGui() const { return mWrapper->hasGui(); }
bool ClapInstance::guiIsApiSupported(const char* api, bool isFloating) const { return mWrapper->guiIsApiSupported(api, isFloating); }
bool ClapInstance::guiCreate(const char* api, bool isFloating) { return mWrapper->guiCreate(api, isFloating); }
void ClapInstance::guiDestroy() { mWrapper->guiDestroy(); }
bool ClapInstance::guiSetScale(double scale) { return mWrapper->guiSetScale(scale); }
bool ClapInstance::guiGetSize(uint32_t& w, uint32_t& h) const { return mWrapper->guiGetSize(w, h); }
bool ClapInstance::guiCanResize() const { return mWrapper->guiCanResize(); }
bool ClapInstance::guiAdjustSize(uint32_t& w, uint32_t& h) const { return mWrapper->guiAdjustSize(w, h); }
bool ClapInstance::guiSetSize(uint32_t w, uint32_t h) { return mWrapper->guiSetSize(w, h); }
bool ClapInstance::guiSetParent(const char* api, void* nativeHandle) { return mWrapper->guiSetParent(api, nativeHandle); }
bool ClapInstance::guiShow() { return mWrapper->guiShow(); }
bool ClapInstance::guiHide() { return mWrapper->guiHide(); }
void ClapInstance::fireTimer(uint32_t timerId) { mWrapper->fireTimer(timerId); }
void ClapInstance::fireFd(int fd, uint32_t flags) { mWrapper->fireFd(fd, flags); }
