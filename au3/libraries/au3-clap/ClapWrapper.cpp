/**********************************************************************

  Audacity: A Digital Audio Editor

  @file ClapWrapper.cpp

**********************************************************************/
#include "ClapWrapper.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

#include <wx/log.h>

#include "au3-components/EffectAutomationParameters.h"
#include "au3-module-manager/ConfigInterface.h"

#include "ClapEntry.h"
#include "ClapSettings.h"

namespace {
constexpr auto valuesKey = wxT("Values");
constexpr auto chunkKey = wxT("Chunk");

ClapEffectSettings& GetSettings(EffectSettings& settings)
{
    auto* p = settings.cast<ClapEffectSettings>();
    assert(p);
    return *p;
}

const ClapEffectSettings& GetSettings(const EffectSettings& settings)
{
    return GetSettings(const_cast<EffectSettings&>(settings));
}

//! Serialize "id:value;id:value;..." for storage in CommandParameters / config.
wxString ValuesToString(const std::map<uint32_t, double>& values)
{
    wxString result;
    for (const auto& [id, value] : values) {
        result += wxString::Format(wxT("%u:%.17g;"), id, value);
    }
    return result;
}

std::map<uint32_t, double> ValuesFromString(const wxString& str)
{
    std::map<uint32_t, double> values;
    wxString rest = str;
    while (!rest.empty()) {
        const wxString token = rest.BeforeFirst(';', &rest);
        if (token.empty()) {
            continue;
        }
        wxString valueStr;
        const wxString idStr = token.BeforeFirst(':', &valueStr);
        unsigned long id = 0;
        double value = 0.0;
        if (idStr.ToULong(&id) && valueStr.ToCDouble(&value)) {
            values[static_cast<uint32_t>(id)] = value;
        }
    }
    return values;
}

wxString ToHex(const std::vector<uint8_t>& data)
{
    static const char* digits = "0123456789abcdef";
    wxString out;
    out.Alloc(data.size() * 2);
    for (uint8_t b : data) {
        out += digits[b >> 4];
        out += digits[b & 0x0f];
    }
    return out;
}

int hexNibble(wxChar c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

std::vector<uint8_t> FromHex(const wxString& str)
{
    std::vector<uint8_t> out;
    out.reserve(str.size() / 2);
    for (size_t i = 0; i + 1 < str.size(); i += 2) {
        const int hi = hexNibble(str[i]);
        const int lo = hexNibble(str[i + 1]);
        if (hi < 0 || lo < 0) {
            break;
        }
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return out;
}

//! Minimal clap_ostream collecting bytes into a vector.
struct VectorOStream {
    std::vector<uint8_t> data;
    clap_ostream_t stream;

    VectorOStream()
    {
        stream.ctx = this;
        stream.write = [](const clap_ostream_t* s, const void* buffer, uint64_t size) -> int64_t {
            auto* self = static_cast<VectorOStream*>(s->ctx);
            const auto* p = static_cast<const uint8_t*>(buffer);
            self->data.insert(self->data.end(), p, p + size);
            return static_cast<int64_t>(size);
        };
    }
};

//! Minimal clap_istream reading from a vector.
struct VectorIStream {
    const std::vector<uint8_t>& data;
    size_t pos { 0 };
    clap_istream_t stream;

    explicit VectorIStream(const std::vector<uint8_t>& d)
        : data(d)
    {
        stream.ctx = this;
        stream.read = [](const clap_istream_t* s, void* buffer, uint64_t size) -> int64_t {
            auto* self = static_cast<VectorIStream*>(s->ctx);
            const size_t avail = self->data.size() - self->pos;
            const size_t n = std::min<size_t>(size, avail);
            std::memcpy(buffer, self->data.data() + self->pos, n);
            self->pos += n;
            return static_cast<int64_t>(n);
        };
    }
};

void pushParamValue(clap::helpers::EventList& list, clap_id id, double value)
{
    clap_event_param_value_t ev {};
    ev.header.size = sizeof(ev);
    ev.header.time = 0;
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.type = CLAP_EVENT_PARAM_VALUE;
    ev.header.flags = 0;
    ev.param_id = id;
    ev.cookie = nullptr;
    ev.note_id = -1;
    ev.port_index = -1;
    ev.channel = -1;
    ev.key = -1;
    ev.value = value;
    list.push(&ev.header);
}
}

ClapWrapper::ClapWrapper(std::shared_ptr<ClapEntry> entry, std::string pluginId)
    : mEntry(std::move(entry)), mPluginId(std::move(pluginId))
{
    mHost.clap_version = CLAP_VERSION;
    mHost.host_data = this;
    mHost.name = "Audacity";
    mHost.vendor = "The Audacity Team";
    mHost.url = "https://www.audacityteam.org";
    mHost.version = "1.0";
    mHost.get_extension = &ClapWrapper::hostGetExtension;
    mHost.request_restart = &ClapWrapper::hostRequestRestart;
    mHost.request_process = &ClapWrapper::hostRequestProcess;
    mHost.request_callback = &ClapWrapper::hostRequestCallback;

    mHostLog.log = &ClapWrapper::hostLog;
    mHostThreadCheck.is_main_thread = &ClapWrapper::hostIsMainThread;
    mHostThreadCheck.is_audio_thread = &ClapWrapper::hostIsAudioThread;
    mHostParams.rescan = &ClapWrapper::hostParamsRescan;
    mHostParams.clear = &ClapWrapper::hostParamsClear;
    mHostParams.request_flush = &ClapWrapper::hostParamsRequestFlush;
    mHostLatency.changed = &ClapWrapper::hostLatencyChanged;
    mHostState.mark_dirty = &ClapWrapper::hostStateMarkDirty;

    mHostGui.resize_hints_changed = &ClapWrapper::hostGuiResizeHintsChanged;
    mHostGui.request_resize = &ClapWrapper::hostGuiRequestResize;
    mHostGui.request_show = &ClapWrapper::hostGuiRequestShow;
    mHostGui.request_hide = &ClapWrapper::hostGuiRequestHide;
    mHostGui.closed = &ClapWrapper::hostGuiClosed;

    mHostTimerSupport.register_timer = &ClapWrapper::hostTimerRegister;
    mHostTimerSupport.unregister_timer = &ClapWrapper::hostTimerUnregister;

    mHostPosixFd.register_fd = &ClapWrapper::hostPosixFdRegister;
    mHostPosixFd.modify_fd = &ClapWrapper::hostPosixFdModify;
    mHostPosixFd.unregister_fd = &ClapWrapper::hostPosixFdUnregister;
}

ClapWrapper::~ClapWrapper()
{
    if (mPlugin) {
        if (mActive) {
            Finalize(nullptr);
        }
        mPlugin->destroy(mPlugin);
        mPlugin = nullptr;
    }
}

void ClapWrapper::InitializeComponents()
{
    const auto* factory = mEntry->factory();
    mPlugin = factory->create_plugin(factory, &mHost, mPluginId.c_str());
    if (!mPlugin) {
        throw std::runtime_error("clap create_plugin failed");
    }
    if (!mPlugin->init(mPlugin)) {
        mPlugin->destroy(mPlugin);
        mPlugin = nullptr;
        throw std::runtime_error("clap plugin init failed");
    }

    mParams = static_cast<const clap_plugin_params_t*>(mPlugin->get_extension(mPlugin, CLAP_EXT_PARAMS));
    mAudioPorts = static_cast<const clap_plugin_audio_ports_t*>(mPlugin->get_extension(mPlugin, CLAP_EXT_AUDIO_PORTS));
    mState = static_cast<const clap_plugin_state_t*>(mPlugin->get_extension(mPlugin, CLAP_EXT_STATE));
    mLatency = static_cast<const clap_plugin_latency_t*>(mPlugin->get_extension(mPlugin, CLAP_EXT_LATENCY));
    mGui = static_cast<const clap_plugin_gui_t*>(mPlugin->get_extension(mPlugin, CLAP_EXT_GUI));
    mPluginTimerSupport = static_cast<const clap_plugin_timer_support_t*>(mPlugin->get_extension(mPlugin, CLAP_EXT_TIMER_SUPPORT));
    mPluginPosixFd = static_cast<const clap_plugin_posix_fd_support_t*>(mPlugin->get_extension(mPlugin, CLAP_EXT_POSIX_FD_SUPPORT));

    scanAudioPorts();
}

void ClapWrapper::scanAudioPorts()
{
    mInPortChannels.clear();
    mOutPortChannels.clear();
    mMainInPort = -1;
    mMainOutPort = -1;
    mAudioIns = 0;
    mAudioOuts = 0;
    if (!mAudioPorts || !mPlugin) {
        return;
    }

    const auto scan = [this](bool isInput, std::vector<uint32_t>& channels, int& mainPort, unsigned& mainCount) {
        const uint32_t n = mAudioPorts->count(mPlugin, isInput);
        channels.resize(n, 0);
        for (uint32_t i = 0; i < n; ++i) {
            clap_audio_port_info_t info {};
            if (!mAudioPorts->get(mPlugin, i, isInput, &info)) {
                continue;
            }
            channels[i] = info.channel_count;
            if ((info.flags & CLAP_AUDIO_PORT_IS_MAIN) && mainPort < 0) {
                mainPort = static_cast<int>(i);
            }
        }
        // The spec puts the main port at index 0; fall back to it if no flag was set.
        if (mainPort < 0 && n > 0) {
            mainPort = 0;
        }
        mainCount = mainPort >= 0 ? channels[mainPort] : 0;
    };

    scan(true, mInPortChannels, mMainInPort, mAudioIns);
    scan(false, mOutPortChannels, mMainOutPort, mAudioOuts);
}

void ClapWrapper::FetchSettings(EffectSettings& settings)
{
    const auto& s = GetSettings(settings);
    if (!s.chunk.empty() && mState) {
        applyChunk(s.chunk);
    } else {
        std::lock_guard<std::mutex> lock(mPendingMutex);
        for (const auto& [id, value] : s.values) {
            mPending[id] = value;
        }
    }
}

void ClapWrapper::StoreSettings(EffectSettings& settings) const
{
    auto& s = GetSettings(settings);
    s.values.clear();
    if (mParams && mPlugin) {
        const uint32_t n = mParams->count(mPlugin);
        for (uint32_t i = 0; i < n; ++i) {
            clap_param_info_t info {};
            if (!mParams->get_info(mPlugin, i, &info)) {
                continue;
            }
            double value = 0.0;
            if (mParams->get_value(mPlugin, info.id, &value)) {
                s.values[info.id] = value;
            }
        }
    }
    s.chunk = saveChunk();
}

void ClapWrapper::applyChunk(const std::vector<uint8_t>& chunk)
{
    if (!mState || !mPlugin || chunk.empty()) {
        return;
    }
    VectorIStream in(chunk);
    mState->load(mPlugin, &in.stream);
}

std::vector<uint8_t> ClapWrapper::saveChunk() const
{
    std::vector<uint8_t> result;
    if (mState && mPlugin) {
        VectorOStream out;
        if (mState->save(mPlugin, &out.stream)) {
            result = std::move(out.data);
        }
    }
    return result;
}

bool ClapWrapper::Initialize(EffectSettings& settings, double sampleRate, size_t maxBlockSize)
{
    if (!mPlugin) {
        return false;
    }

    mSampleRate = sampleRate;
    mMaxBlockSize = static_cast<uint32_t>(std::max<size_t>(1, maxBlockSize));

    scanAudioPorts();
    FetchSettings(settings);

    if (!mPlugin->activate(mPlugin, mSampleRate, 1, mMaxBlockSize)) {
        return false;
    }
    mActive = true;

    mSilence.assign(mMaxBlockSize, 0.0f);
    mScratchOut.assign(mMaxBlockSize, 0.0f);

    mInBuses.assign(mInPortChannels.size(), clap_audio_buffer_t {});
    mInPortPtrs.assign(mInPortChannels.size(), {});
    for (size_t port = 0; port < mInPortChannels.size(); ++port) {
        mInPortPtrs[port].assign(mInPortChannels[port], nullptr);
        mInBuses[port].channel_count = mInPortChannels[port];
    }
    mOutBuses.assign(mOutPortChannels.size(), clap_audio_buffer_t {});
    mOutPortPtrs.assign(mOutPortChannels.size(), {});
    for (size_t port = 0; port < mOutPortChannels.size(); ++port) {
        mOutPortPtrs[port].assign(mOutPortChannels[port], nullptr);
        mOutBuses[port].channel_count = mOutPortChannels[port];
    }

    const auto reserve = (mParams ? mParams->count(mPlugin) : 0u) + 16u;
    mInputEvents.reserveEvents(reserve);
    mInputEvents.reserveHeap(reserve * sizeof(clap_event_param_value_t));
    mOutputEvents.reserveEvents(reserve);
    mOutputEvents.reserveHeap(reserve * sizeof(clap_event_param_value_t));

    mSteadyTime = 0;

    mProcessing = true;
    mStarted = mPlugin->start_processing(mPlugin);
    mProcessing = false;
    if (!mStarted) {
        mPlugin->deactivate(mPlugin);
        mActive = false;
        return false;
    }
    return true;
}

void ClapWrapper::Finalize(EffectSettings* settings)
{
    if (!mPlugin || !mActive) {
        return;
    }
    if (mStarted) {
        mProcessing = true;
        mPlugin->stop_processing(mPlugin);
        mProcessing = false;
        mStarted = false;
    }
    if (settings) {
        StoreSettings(*settings);
    }
    mPlugin->deactivate(mPlugin);
    mActive = false;
}

void ClapWrapper::ProcessBlockStart(const EffectSettings& settings)
{
    const auto& s = GetSettings(settings);
    std::lock_guard<std::mutex> lock(mPendingMutex);
    for (const auto& [id, value] : s.values) {
        const auto it = mLastValues.find(id);
        if (it == mLastValues.end() || it->second != value) {
            mPending[id] = value;
            mLastValues[id] = value;
        }
    }
}

void ClapWrapper::enqueueParamEvents()
{
    std::lock_guard<std::mutex> lock(mPendingMutex);
    for (const auto& [id, value] : mPending) {
        pushParamValue(mInputEvents, id, value);
    }
    mPending.clear();
}

size_t ClapWrapper::Process(const float* const* inBlock, float* const* outBlock, size_t blockLen)
{
    if (!mActive || !mStarted || !mPlugin) {
        return 0;
    }

    mProcessing = true;

    mInputEvents.clear();
    enqueueParamEvents();

    // Map the main ports onto Audacity's buffers; auxiliary ports (e.g. side-chain)
    // read silence and write to a discarded scratch buffer.
    for (size_t port = 0; port < mInBuses.size(); ++port) {
        for (uint32_t c = 0; c < mInPortChannels[port]; ++c) {
            mInPortPtrs[port][c] = (static_cast<int>(port) == mMainInPort && c < mAudioIns)
                                   ? const_cast<float*>(inBlock[c])
                                   : mSilence.data();
        }
        mInBuses[port].data32 = mInPortPtrs[port].empty() ? nullptr : mInPortPtrs[port].data();
        mInBuses[port].constant_mask = 0;
    }
    for (size_t port = 0; port < mOutBuses.size(); ++port) {
        for (uint32_t c = 0; c < mOutPortChannels[port]; ++c) {
            mOutPortPtrs[port][c] = (static_cast<int>(port) == mMainOutPort && c < mAudioOuts)
                                    ? outBlock[c]
                                    : mScratchOut.data();
        }
        mOutBuses[port].data32 = mOutPortPtrs[port].empty() ? nullptr : mOutPortPtrs[port].data();
        mOutBuses[port].constant_mask = 0;
    }

    clap_process_t p {};
    p.steady_time = mSteadyTime;
    p.frames_count = static_cast<uint32_t>(blockLen);
    p.transport = nullptr;
    p.audio_inputs = mInBuses.empty() ? nullptr : mInBuses.data();
    p.audio_inputs_count = static_cast<uint32_t>(mInBuses.size());
    p.audio_outputs = mOutBuses.empty() ? nullptr : mOutBuses.data();
    p.audio_outputs_count = static_cast<uint32_t>(mOutBuses.size());
    p.in_events = mInputEvents.clapInputEvents();
    p.out_events = mOutputEvents.clapOutputEvents();

    const clap_process_status status = mPlugin->process(mPlugin, &p);

    mSteadyTime += static_cast<int64_t>(blockLen);
    mInputEvents.clear();
    mOutputEvents.clear();

    mProcessing = false;

    return status == CLAP_PROCESS_ERROR ? 0 : blockLen;
}

void ClapWrapper::SuspendProcessing()
{
    if (mActive && mStarted) {
        mProcessing = true;
        mPlugin->stop_processing(mPlugin);
        mProcessing = false;
        mStarted = false;
    }
}

void ClapWrapper::ResumeProcessing()
{
    if (mActive && !mStarted) {
        mProcessing = true;
        mStarted = mPlugin->start_processing(mPlugin);
        mProcessing = false;
    }
}

int64_t ClapWrapper::GetLatencySamples() const
{
    if (mLatency && mActive && mPlugin) {
        return static_cast<int64_t>(mLatency->get(mPlugin));
    }
    return 0;
}

uint32_t ClapWrapper::GetParameterCount() const
{
    return (mParams && mPlugin) ? mParams->count(mPlugin) : 0u;
}

bool ClapWrapper::GetParameterInfo(uint32_t index, clap_param_info_t& info) const
{
    return mParams && mPlugin && mParams->get_info(mPlugin, index, &info);
}

bool ClapWrapper::GetParameterValue(clap_id id, double& out) const
{
    return mParams && mPlugin && mParams->get_value(mPlugin, id, &out);
}

bool ClapWrapper::ParameterValueToText(clap_id id, double value, std::string& out) const
{
    if (!mParams || !mPlugin || !mParams->value_to_text) {
        return false;
    }
    char buffer[256] = { 0 };
    if (!mParams->value_to_text(mPlugin, id, value, buffer, sizeof(buffer))) {
        return false;
    }
    out = buffer;
    return true;
}

bool ClapWrapper::ParameterTextToValue(clap_id id, const std::string& text, double& out) const
{
    return mParams && mPlugin && mParams->text_to_value
           && mParams->text_to_value(mPlugin, id, text.c_str(), &out);
}

void ClapWrapper::SetParameterValue(clap_id id, double value, EffectSettings* settings)
{
    if (settings) {
        GetSettings(*settings).values[id] = value;
    }
    {
        std::lock_guard<std::mutex> lock(mPendingMutex);
        mPending[id] = value;
        mLastValues[id] = value;
    }
    if (!mActive) {
        // Safe to flush on the main thread while the plugin is not processing.
        FlushParameters();
    }
    // While active, the queued change is delivered by the next Process() call.
}

void ClapWrapper::FlushParameters()
{
    if (!mPlugin || !mParams || !mParams->flush || mActive) {
        return;
    }
    mInputEvents.clear();
    enqueueParamEvents();
    mParams->flush(mPlugin, mInputEvents.clapInputEvents(), mOutputEvents.clapOutputEvents());
    mInputEvents.clear();
    mOutputEvents.clear();
}

// ===========================================================================
// clap_host callbacks
// ===========================================================================

const void* ClapWrapper::hostGetExtension(const clap_host_t* host, const char* id)
{
    auto* self = from(host);
    if (std::strcmp(id, CLAP_EXT_LOG) == 0) {
        return &self->mHostLog;
    }
    if (std::strcmp(id, CLAP_EXT_THREAD_CHECK) == 0) {
        return &self->mHostThreadCheck;
    }
    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) {
        return &self->mHostParams;
    }
    if (std::strcmp(id, CLAP_EXT_LATENCY) == 0) {
        return &self->mHostLatency;
    }
    if (std::strcmp(id, CLAP_EXT_STATE) == 0) {
        return &self->mHostState;
    }
    if (std::strcmp(id, CLAP_EXT_GUI) == 0) {
        return &self->mHostGui;
    }
    if (std::strcmp(id, CLAP_EXT_TIMER_SUPPORT) == 0) {
        return &self->mHostTimerSupport;
    }
    if (std::strcmp(id, CLAP_EXT_POSIX_FD_SUPPORT) == 0) {
        return &self->mHostPosixFd;
    }
    return nullptr;
}

void ClapWrapper::hostRequestRestart(const clap_host_t*) {}
void ClapWrapper::hostRequestProcess(const clap_host_t*) {}
void ClapWrapper::hostRequestCallback(const clap_host_t*) {}

void ClapWrapper::hostLog(const clap_host_t* host, clap_log_severity severity, const char* msg)
{
    switch (severity) {
    case CLAP_LOG_ERROR:
    case CLAP_LOG_FATAL:
    case CLAP_LOG_PLUGIN_MISBEHAVING:
    case CLAP_LOG_HOST_MISBEHAVING:
        wxLogError("CLAP: %s", msg);
        break;
    case CLAP_LOG_WARNING:
        wxLogWarning("CLAP: %s", msg);
        break;
    default:
        wxLogDebug("CLAP: %s", msg);
        break;
    }
}

bool ClapWrapper::hostIsMainThread(const clap_host_t* host)
{
    // Approximation: any call made while not inside Process() is treated as the
    // main thread. This avoids OS-thread-id mismatches in Audacity's worker model.
    return !from(host)->mProcessing.load();
}

bool ClapWrapper::hostIsAudioThread(const clap_host_t* host)
{
    return from(host)->mProcessing.load();
}

void ClapWrapper::hostParamsRescan(const clap_host_t*, clap_param_rescan_flags) {}
void ClapWrapper::hostParamsClear(const clap_host_t*, clap_id, clap_param_clear_flags) {}
void ClapWrapper::hostParamsRequestFlush(const clap_host_t*) {}
void ClapWrapper::hostLatencyChanged(const clap_host_t*) {}
void ClapWrapper::hostStateMarkDirty(const clap_host_t*) {}

void ClapWrapper::hostGuiResizeHintsChanged(const clap_host_t* host)
{
    if (auto* l = from(host)->mListener) {
        l->guiResizeHintsChanged();
    }
}

bool ClapWrapper::hostGuiRequestResize(const clap_host_t* host, uint32_t width, uint32_t height)
{
    auto* l = from(host)->mListener;
    return l ? l->guiRequestResize(width, height) : false;
}

bool ClapWrapper::hostGuiRequestShow(const clap_host_t* host)
{
    auto* l = from(host)->mListener;
    return l ? l->guiRequestShow() : false;
}

bool ClapWrapper::hostGuiRequestHide(const clap_host_t* host)
{
    auto* l = from(host)->mListener;
    return l ? l->guiRequestHide() : false;
}

void ClapWrapper::hostGuiClosed(const clap_host_t* host, bool wasDestroyed)
{
    if (auto* l = from(host)->mListener) {
        l->guiClosed(wasDestroyed);
    }
}

bool ClapWrapper::hostTimerRegister(const clap_host_t* host, uint32_t periodMs, clap_id* outTimerId)
{
    auto* l = from(host)->mListener;
    if (!l || !outTimerId) {
        return false;
    }
    uint32_t id = 0;
    if (!l->registerTimer(periodMs, id)) {
        return false;
    }
    *outTimerId = static_cast<clap_id>(id);
    return true;
}

bool ClapWrapper::hostTimerUnregister(const clap_host_t* host, clap_id timerId)
{
    auto* l = from(host)->mListener;
    return l ? l->unregisterTimer(static_cast<uint32_t>(timerId)) : false;
}

bool ClapWrapper::hostPosixFdRegister(const clap_host_t* host, int fd, clap_posix_fd_flags_t flags)
{
    auto* l = from(host)->mListener;
    return l ? l->registerFd(fd, static_cast<uint32_t>(flags)) : false;
}

bool ClapWrapper::hostPosixFdModify(const clap_host_t* host, int fd, clap_posix_fd_flags_t flags)
{
    auto* l = from(host)->mListener;
    return l ? l->modifyFd(fd, static_cast<uint32_t>(flags)) : false;
}

bool ClapWrapper::hostPosixFdUnregister(const clap_host_t* host, int fd)
{
    auto* l = from(host)->mListener;
    return l ? l->unregisterFd(fd) : false;
}

// ===========================================================================
// GUI public methods (forward to mGui after null-checking)
// ===========================================================================

bool ClapWrapper::guiIsApiSupported(const char* api, bool isFloating) const
{
    return mGui && mPlugin && mGui->is_api_supported && mGui->is_api_supported(mPlugin, api, isFloating);
}

bool ClapWrapper::guiCreate(const char* api, bool isFloating)
{
    return mGui && mPlugin && mGui->create && mGui->create(mPlugin, api, isFloating);
}

void ClapWrapper::guiDestroy()
{
    if (mGui && mPlugin && mGui->destroy) {
        mGui->destroy(mPlugin);
    }
}

bool ClapWrapper::guiSetScale(double scale)
{
    return mGui && mPlugin && mGui->set_scale && mGui->set_scale(mPlugin, scale);
}

bool ClapWrapper::guiGetSize(uint32_t& w, uint32_t& h) const
{
    return mGui && mPlugin && mGui->get_size && mGui->get_size(mPlugin, &w, &h);
}

bool ClapWrapper::guiCanResize() const
{
    return mGui && mPlugin && mGui->can_resize && mGui->can_resize(mPlugin);
}

bool ClapWrapper::guiAdjustSize(uint32_t& w, uint32_t& h) const
{
    return mGui && mPlugin && mGui->adjust_size && mGui->adjust_size(mPlugin, &w, &h);
}

bool ClapWrapper::guiSetSize(uint32_t w, uint32_t h)
{
    return mGui && mPlugin && mGui->set_size && mGui->set_size(mPlugin, w, h);
}

bool ClapWrapper::guiSetParent(const char* api, void* nativeHandle)
{
    if (!mGui || !mPlugin || !mGui->set_parent) {
        return false;
    }
    clap_window_t window {};
    window.api = api;
    // The union members are all pointers / unsigned long; storing into `ptr`
    // covers cocoa/uikit/win32 and the upper bits of x11 are zero on 64-bit
    // platforms. For X11 we set the x11 field explicitly.
    if (api && std::strcmp(api, CLAP_WINDOW_API_X11) == 0) {
        window.x11 = reinterpret_cast<clap_xwnd>(nativeHandle);
    } else {
        window.ptr = nativeHandle;
    }
    return mGui->set_parent(mPlugin, &window);
}

bool ClapWrapper::guiShow()
{
    return mGui && mPlugin && mGui->show && mGui->show(mPlugin);
}

bool ClapWrapper::guiHide()
{
    return mGui && mPlugin && mGui->hide && mGui->hide(mPlugin);
}

void ClapWrapper::fireTimer(uint32_t timerId)
{
    if (mPluginTimerSupport && mPlugin && mPluginTimerSupport->on_timer) {
        mPluginTimerSupport->on_timer(mPlugin, static_cast<clap_id>(timerId));
    }
}

void ClapWrapper::fireFd(int fd, uint32_t flags)
{
    if (mPluginPosixFd && mPlugin && mPluginPosixFd->on_fd) {
        mPluginPosixFd->on_fd(mPlugin, fd, static_cast<clap_posix_fd_flags_t>(flags));
    }
}

// ===========================================================================
// Settings statics
// ===========================================================================

EffectSettings ClapWrapper::MakeSettings()
{
    return EffectSettings::Make<ClapEffectSettings>();
}

void ClapWrapper::CopySettingsContents(const EffectSettings& src, EffectSettings& dst)
{
    EffectSettings::Copy<ClapEffectSettings>(src, dst);
}

void ClapWrapper::SaveSettings(const EffectSettings& settings, CommandParameters& parms)
{
    const auto& s = GetSettings(settings);
    if (!s.values.empty()) {
        parms.Write(valuesKey, ValuesToString(s.values));
    }
    if (!s.chunk.empty()) {
        parms.Write(chunkKey, ToHex(s.chunk));
    }
}

void ClapWrapper::LoadSettings(const CommandParameters& parms, EffectSettings& settings)
{
    ClapEffectSettings s;
    if (parms.HasEntry(valuesKey)) {
        s.values = ValuesFromString(parms.Read(valuesKey));
    }
    if (parms.HasEntry(chunkKey)) {
        s.chunk = FromHex(parms.Read(chunkKey));
    }
    GetSettings(settings) = std::move(s);
}

OptionalMessage ClapWrapper::LoadUserPreset(
    const EffectDefinitionInterface& effect, const RegistryPath& name, EffectSettings& settings)
{
    ClapEffectSettings s;
    wxString valuesStr;
    if (GetConfig(effect, PluginSettings::Private, name, valuesKey, valuesStr, wxEmptyString)) {
        s.values = ValuesFromString(valuesStr);
    }
    wxString chunkStr;
    if (GetConfig(effect, PluginSettings::Private, name, chunkKey, chunkStr, wxEmptyString)) {
        s.chunk = FromHex(chunkStr);
    }
    GetSettings(settings) = std::move(s);
    return { nullptr };
}

void ClapWrapper::SaveUserPreset(
    const EffectDefinitionInterface& effect, const RegistryPath& name, const EffectSettings& settings)
{
    const auto& s = GetSettings(settings);
    if (!s.values.empty()) {
        SetConfig(effect, PluginSettings::Private, name, valuesKey, ValuesToString(s.values));
    }
    if (!s.chunk.empty()) {
        SetConfig(effect, PluginSettings::Private, name, chunkKey, ToHex(s.chunk));
    }
}
