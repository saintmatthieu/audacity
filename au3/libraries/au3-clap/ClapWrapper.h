/**********************************************************************

  Audacity: A Digital Audio Editor

  @file ClapWrapper.h

  @brief Host-side wrapper around a single CLAP plugin instance.

  Mirrors the role of VST3Wrapper: it owns the clap_host callbacks, creates and
  initializes the plugin, drives the process loop and bridges Audacity's
  EffectSettings to the plugin's parameters and opaque state.

  This header includes the CLAP SDK and must therefore only be used internally
  by the au3-clap module (the public au3-clap headers stay SDK-free).

**********************************************************************/
#pragma once

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <clap/clap.h>
#include <clap/helpers/event-list.hh>

#include "au3-components/EffectInterface.h"

class ClapEntry;
class CommandParameters;

class ClapWrapper final
{
public:
    ClapWrapper(std::shared_ptr<ClapEntry> entry, std::string pluginId);
    ~ClapWrapper();

    ClapWrapper(const ClapWrapper&) = delete;
    ClapWrapper& operator=(const ClapWrapper&) = delete;

    //! Create and init the underlying plugin. Throws std::runtime_error on failure.
    //! Must be called once before any other method.
    void InitializeComponents();

    bool IsActive() const noexcept { return mActive; }

    const clap_plugin_t* plugin() const { return mPlugin; }
    const std::string& pluginId() const { return mPluginId; }
    std::shared_ptr<ClapEntry> entry() const { return mEntry; }

    //! Apply persisted state (chunk or parameter values) from \p settings onto the plugin.
    void FetchSettings(EffectSettings& settings);
    //! Read the current plugin state back into \p settings.
    void StoreSettings(EffectSettings& settings) const;

    //! Activate + start processing using \p settings. Returns false on failure.
    bool Initialize(EffectSettings& settings, double sampleRate, size_t maxBlockSize);
    //! Stop + deactivate. When \p settings is non-null the current state is stored first.
    void Finalize(EffectSettings* settings);

    //! Deliver any pending parameter changes carried by \p settings before a block.
    void ProcessBlockStart(const EffectSettings& settings);

    //! Process one block; returns the number of frames produced.
    size_t Process(const float* const* inBlock, float* const* outBlock, size_t blockLen);

    void SuspendProcessing();
    void ResumeProcessing();

    unsigned GetAudioInCount() const { return mAudioIns; }
    unsigned GetAudioOutCount() const { return mAudioOuts; }
    int64_t GetLatencySamples() const;

    // ---- Parameter access (used by the auto-generated UI / extractor) --------
    uint32_t GetParameterCount() const;
    bool GetParameterInfo(uint32_t index, clap_param_info_t& info) const;
    bool GetParameterValue(clap_id id, double& out) const;
    bool ParameterValueToText(clap_id id, double value, std::string& out) const;
    bool ParameterTextToValue(clap_id id, const std::string& text, double& out) const;
    //! Queue a plain-value change for \p id, applied on the next process/flush.
    //! When \p settings is non-null the value is also persisted into it.
    void SetParameterValue(clap_id id, double value, EffectSettings* settings);
    //! Push pending changes to the plugin while it is not processing.
    void FlushParameters();

    // ---- Settings statics (mirror VST3Wrapper) -------------------------------
    static EffectSettings MakeSettings();
    static void CopySettingsContents(const EffectSettings& src, EffectSettings& dst);
    static void SaveSettings(const EffectSettings& settings, CommandParameters& parms);
    static void LoadSettings(const CommandParameters& parms, EffectSettings& settings);
    static OptionalMessage LoadUserPreset(
        const EffectDefinitionInterface& effect, const RegistryPath& name, EffectSettings& settings);
    static void SaveUserPreset(
        const EffectDefinitionInterface& effect, const RegistryPath& name, const EffectSettings& settings);

private:
    // clap_host callbacks
    static const void* hostGetExtension(const clap_host_t* host, const char* id);
    static void hostRequestRestart(const clap_host_t* host);
    static void hostRequestProcess(const clap_host_t* host);
    static void hostRequestCallback(const clap_host_t* host);
    static void hostLog(const clap_host_t* host, clap_log_severity severity, const char* msg);
    static bool hostIsMainThread(const clap_host_t* host);
    static bool hostIsAudioThread(const clap_host_t* host);
    static void hostParamsRescan(const clap_host_t* host, clap_param_rescan_flags flags);
    static void hostParamsClear(const clap_host_t* host, clap_id paramId, clap_param_clear_flags flags);
    static void hostParamsRequestFlush(const clap_host_t* host);
    static void hostLatencyChanged(const clap_host_t* host);
    static void hostStateMarkDirty(const clap_host_t* host);

    static ClapWrapper* from(const clap_host_t* host) { return static_cast<ClapWrapper*>(host->host_data); }

    void scanAudioPorts();
    void applyChunk(const std::vector<uint8_t>& chunk);
    std::vector<uint8_t> saveChunk() const;
    void enqueueParamEvents(); //!< move mPending into the input event list

    std::shared_ptr<ClapEntry> mEntry;
    std::string mPluginId;

    clap_host_t mHost {};
    clap_host_log_t mHostLog {};
    clap_host_thread_check_t mHostThreadCheck {};
    clap_host_params_t mHostParams {};
    clap_host_latency_t mHostLatency {};
    clap_host_state_t mHostState {};

    const clap_plugin_t* mPlugin { nullptr };
    const clap_plugin_params_t* mParams { nullptr };
    const clap_plugin_audio_ports_t* mAudioPorts { nullptr };
    const clap_plugin_state_t* mState { nullptr };
    const clap_plugin_latency_t* mLatency { nullptr };

    bool mActive { false };
    bool mStarted { false };
    std::atomic<bool> mProcessing { false };

    double mSampleRate { 44100.0 };
    uint32_t mMaxBlockSize { 8192 };
    unsigned mAudioIns { 0 };  //!< main input port channel count (what Audacity provides)
    unsigned mAudioOuts { 0 }; //!< main output port channel count

    // Full audio-port layout. CLAP requires the host to present *every* declared
    // port each process call, so auxiliary ports are fed silence / scratch.
    std::vector<uint32_t> mInPortChannels;
    std::vector<uint32_t> mOutPortChannels;
    int mMainInPort { -1 };
    int mMainOutPort { -1 };

    clap::helpers::EventList mInputEvents;
    clap::helpers::EventList mOutputEvents;
    std::vector<clap_audio_buffer_t> mInBuses;
    std::vector<clap_audio_buffer_t> mOutBuses;
    std::vector<std::vector<float*> > mInPortPtrs;
    std::vector<std::vector<float*> > mOutPortPtrs;
    std::vector<float> mSilence;     //!< shared zero buffer for auxiliary inputs
    std::vector<float> mScratchOut;  //!< shared discard buffer for auxiliary outputs
    int64_t mSteadyTime { 0 };

    std::mutex mPendingMutex;
    std::map<uint32_t, double> mPending;
    //! Last values delivered to the plugin, to send only deltas per realtime block.
    std::map<uint32_t, double> mLastValues;
};
