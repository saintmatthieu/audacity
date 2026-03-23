/*
* Audacity: A Digital Audio Editor
*/
#include "builtineffectsloader.h"

#include <QtQml>

#include "global/translation.h"
#include "global/log.h"

#include "au3-module-manager/PluginManager.h"
#include "au3-effects/LoadEffects.h"

#include "au3wrap/internal/wxtypes_convert.h"

#include "effects/effects_base/effectstypes.h"
#include "effects/effects_base/internal/au3/au3effectsutils.h"
#include "effects/effects_base/internal/effectsutils.h"
#include "effects/effects_base/view/effectsviewutils.h"

#include "amplify/amplifyeffect.h"
#include "amplify/amplifyviewmodel.h"
#include "loudness/normalizeloudnesseffect.h"
#include "loudness/normalizeloudnessviewmodel.h"
#include "clickremoval/clickremovaleffect.h"
#include "clickremoval/clickremovalviewmodel.h"
#include "dynamics/timeline/meters/compressiondbmetermodel.h"
#include "dynamics/timeline/meters/outputdbmetermodel.h"
#include "dynamics/timeline/dynamicstimeline.h"
#include "dynamics/timeline/timelinesourcemodel.h"
#include "dynamics/timeline/stopwatch.h"
#include "dynamics/timeline/dynamicsplaystatemodel.h"
#include "dynamics/compressor/compressoreffect.h"
#include "dynamics/compressor/compressorviewmodel.h"
#include "dynamics/compressor/compressorsettingmodel.h"
#include "dynamics/limiter/limitereffect.h"
#include "dynamics/limiter/limiterviewmodel.h"
#include "dynamics/limiter/limitersettingmodel.h"
#include "normalize/normalizeeffect.h"
#include "normalize/normalizeviewmodel.h"
#include "tonegen/chirpeffect.h"
#include "tonegen/toneeffect.h"
#include "reverb/reverbeffect.h"
#include "reverb/reverbviewmodel.h"
#include "tonegen/toneviewmodel.h"
#include "dtmfgen/dtmfgenerator.h"
#include "dtmfgen/dtmfviewmodel.h"
#include "silencegen/silencegenerator.h"
#include "silencegen/silenceviewmodel.h"
#include "noisegen/noisegenerator.h"
#include "noisegen/noiseviewmodel.h"
#include "noisereduction/noisereductioneffect.h"
#include "noisereduction/noisereductionviewmodel.h"
#include "fade/fadeeffect.h"
#include "graphiceq/graphiceq.h"
#include "graphiceq/graphiceqbandsmodel.h"
#include "graphiceq/graphiceqviewmodel.h"
#include "invert/inverteffect.h"
#include "reverse/reverseeffect.h"
#include "repair/repaireffect.h"
#include "truncatesilence/truncatesilenceeffect.h"
#include "truncatesilence/truncatesilenceviewmodel.h"
#if USE_SOUNDTOUCH
#include "changepitch/changepitcheffect.h"
#include "changepitch/changepitchviewmodel.h"
#endif

#include <algorithm>

using namespace au::effects;

namespace {
void addMeta(const ::PluginDescriptor& desc, const muse::String& title, const muse::String& description,
             bool supportsMultipleClipSelection, EffectMetaList& effects)
{
    IF_ASSERT_FAILED(desc.IsValid()) {
        // If you hit this assertion, tell me, I'm curious. There may be some use case.
        return;
    }

    EffectMeta meta;
    meta.id = au::au3::wxToString(desc.GetID());
    meta.family = EffectFamily::Builtin;
    meta.category = utils::builtinEffectCategoryIdString(toAu4EffectCategory(desc.GetEffectGroup()));
    meta.title = title;
    meta.description = description;
    meta.isEnabled = desc.IsEnabled();
    meta.isRealtimeCapable = desc.IsEffectRealtime();
    meta.supportsMultipleClipSelection = supportsMultipleClipSelection;
    meta.vendor = "Audacity";
    meta.path = desc.GetPath();

    switch (desc.GetEffectType()) {
    case EffectTypeGenerate:
        meta.type = au::effects::EffectType::Generator;
        break;
    case EffectTypeProcess:
        meta.type = au::effects::EffectType::Processor;
        break;
    case EffectTypeAnalyze:
        meta.type = au::effects::EffectType::Analyzer;
        break;
    case EffectTypeTool:
        meta.type = au::effects::EffectType::Tool;
        break;
    default:
        assert(false);
    }

    effects.push_back(std::move(meta));
}
}

void BuiltinEffectsLoader::preInit()
{
    static BuiltinEffectsModule::Registration< FadeInEffect > regFadeIn;
    static BuiltinEffectsModule::Registration< FadeOutEffect > regFadeOut;
    static BuiltinEffectsModule::Registration< InvertEffect > regInvert;
    static BuiltinEffectsModule::Registration< Repair > regRepair;
    static BuiltinEffectsModule::Registration< ReverseEffect > regReverse;
    static BuiltinEffectsModule::Registration< TruncateSilenceEffect > regTruncateSilence;
#if USE_SOUNDTOUCH
    static BuiltinEffectsModule::Registration< ChangePitchEffect > regChangePitch;
#endif
    static BuiltinEffectsModule::Registration< AmplifyEffect > regAmplify;
    static BuiltinEffectsModule::Registration< NormalizeLoudnessEffect > regLoudness;
    static BuiltinEffectsModule::Registration< GraphicEq > regGraphicEq;
    static BuiltinEffectsModule::Registration< ClickRemovalEffect > regClickRemoval;
    static BuiltinEffectsModule::Registration< NormalizeEffect > regNormalize;
    static BuiltinEffectsModule::Registration< ChirpEffect > regChirp;
    static BuiltinEffectsModule::Registration< ToneEffect > regTone;
    static BuiltinEffectsModule::Registration< ReverbEffect > regReverb;
    static BuiltinEffectsModule::Registration< SilenceGenerator > regSilence;
    static BuiltinEffectsModule::Registration< NoiseGenerator > regNoise;
    static BuiltinEffectsModule::Registration< NoiseReductionEffect > regNoiseReduction;
    static BuiltinEffectsModule::Registration< DtmfGenerator > regDtmf;
    static BuiltinEffectsModule::Registration< CompressorEffect > regCompressor;
    static BuiltinEffectsModule::Registration< LimiterEffect > regLimiter;
}

void BuiltinEffectsLoader::regView(const ::ComponentInterfaceSymbol& symbol, const muse::String& url) const
{
    builtinEffectsViewRegister()->regUrl(au3::wxToString(symbol.Internal()), url);
}

void BuiltinEffectsLoader::init()
{
    REGISTER_AUDACITY_EFFECTS_SINGLETON_TYPE(AmplifyViewModelFactory);
    REGISTER_AUDACITY_EFFECTS_SINGLETON_TYPE(NormalizeLoudnessViewModelFactory);
    REGISTER_AUDACITY_EFFECTS_SINGLETON_TYPE(GraphicEqViewModelFactory);
    REGISTER_AUDACITY_EFFECTS_SINGLETON_TYPE(ClickRemovalViewModelFactory);
    REGISTER_AUDACITY_EFFECTS_SINGLETON_TYPE(CompressorViewModelFactory);
    REGISTER_AUDACITY_EFFECTS_SINGLETON_TYPE(CompressorSettingModelFactory);
    REGISTER_AUDACITY_EFFECTS_SINGLETON_TYPE(LimiterViewModelFactory);
    REGISTER_AUDACITY_EFFECTS_SINGLETON_TYPE(LimiterSettingModelFactory);
    REGISTER_AUDACITY_EFFECTS_SINGLETON_TYPE(NormalizeViewModelFactory);
    REGISTER_AUDACITY_EFFECTS_SINGLETON_TYPE(TruncateSilenceViewModelFactory);
    REGISTER_AUDACITY_EFFECTS_SINGLETON_TYPE(ChangePitchViewModelFactory);
    REGISTER_AUDACITY_EFFECTS_SINGLETON_TYPE(ToneViewModelFactory);
    REGISTER_AUDACITY_EFFECTS_SINGLETON_TYPE(ReverbViewModelFactory);
    REGISTER_AUDACITY_EFFECTS_SINGLETON_TYPE(NoiseViewModelFactory);
    REGISTER_AUDACITY_EFFECTS_SINGLETON_TYPE(NoiseReductionViewModelFactory);
    REGISTER_AUDACITY_EFFECTS_SINGLETON_TYPE(DtmfViewModelFactory);
    REGISTER_AUDACITY_EFFECTS_SINGLETON_TYPE(SilenceViewModelFactory);

    regView(AmplifyEffect::Symbol, u"qrc:/amplify/AmplifyView.qml");
    regView(NormalizeLoudnessEffect::Symbol, u"qrc:/loudness/NormalizeLoudnessView.qml");
    regView(GraphicEq::Symbol, u"qrc:/graphiceq/GraphicEqView.qml");
    regView(ClickRemovalEffect::Symbol, u"qrc:/clickremoval/ClickRemovalView.qml");
    regView(NormalizeEffect::Symbol, u"qrc:/normalize/NormalizeView.qml");
    regView(TruncateSilenceEffect::Symbol, u"qrc:/truncatesilence/TruncateSilenceView.qml");
    regView(ChangePitchEffect::Symbol, u"qrc:/changepitch/ChangePitchView.qml");
    regView(ChirpEffect::Symbol, u"qrc:/tonegen/ChirpView.qml");
    regView(ToneEffect::Symbol, u"qrc:/tonegen/ToneView.qml");
    regView(ReverbEffect::Symbol, u"qrc:/reverb/ReverbView.qml");
    regView(NoiseGenerator::Symbol, u"qrc:/noisegen/NoiseView.qml");
    regView(NoiseReductionEffect::Symbol, u"qrc:/noisereduction/NoiseReductionView.qml");
    regView(DtmfGenerator::Symbol, u"qrc:/dtmfgen/DtmfView.qml");
    regView(SilenceGenerator::Symbol, u"qrc:/silencegen/SilenceView.qml");
    regView(LimiterEffect::Symbol, u"qrc:/dynamics/limiter/LimiterView.qml");
    regView(CompressorEffect::Symbol, u"qrc:/dynamics/compressor/CompressorView.qml");

    qmlRegisterType<GraphicEqBandsModel>("Audacity.Effects", 1, 0, "GraphicEqBandsModel");
    qmlRegisterType<DynamicsTimeline>("Audacity.BuiltinEffectsCollection", 1, 0, "DynamicsTimeline");
    qmlRegisterType<TimelineSourceModel>("Audacity.BuiltinEffectsCollection", 1, 0, "TimelineSourceModel");
    qmlRegisterType<CompressionDbMeterModel>("Audacity.BuiltinEffectsCollection", 1, 0, "CompressionDbMeterModel");
    qmlRegisterType<OutputDbMeterModel>("Audacity.BuiltinEffectsCollection", 1, 0, "OutputDbMeterModel");
    qmlRegisterType<Stopwatch>("Audacity.BuiltinEffectsCollection", 1, 0, "Stopwatch");
    qmlRegisterType<DynamicsPlayStateModel>("Audacity.BuiltinEffectsCollection", 1, 0, "DynamicsPlayStateModel");

    m_pluginManagerSubscription = PluginManager::Get().Subscribe([this](PluginsChangedMessage){ m_effectMetaListUpdated.notify(); });
}

EffectMetaList BuiltinEffectsLoader::effectMetaList() const
{
    EffectMetaList effects;

    // All plugins and not PluginsOfType(PluginTypeEffect) to also get disabled effects
    const auto allPlugins = PluginManager::Get().AllPlugins();
    for (const PluginDescriptor& desc : allPlugins) {
        const auto& symbol = desc.GetSymbol();
        if (symbol == AmplifyEffect::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects", "Amplify"),
                    muse::mtrc("effects", "Increases or decreases the volume of the audio you have selected"),
                    false,
                    effects);
        } else if (symbol == NormalizeLoudnessEffect::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects", "Loudness normalization"),
                    muse::mtrc("effects", "Sets the loudness of one or more tracks"),
                    true,
                    effects);
        } else if (symbol == GraphicEq::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects", "Graphic EQ"),
                    muse::mtrc("effects", "Adjusts the balance between frequency components"),
                    true,
                    effects);
        } else if (symbol == ClickRemovalEffect::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects", "Click removal"),
                    muse::mtrc("effects", "Click removal is designed to remove clicks on audio tracks"),
                    true,
                    effects);
        } else if (symbol == CompressorEffect::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects", "Compressor"),
                    muse::mtrc("effects", "Reduces “dynamic range”, or differences between loud and quiet parts"),
                    true,
                    effects);
        } else if (symbol == LimiterEffect::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects", "Limiter"),
                    muse::mtrc("effects", "Augments loudness while minimizing distortion"),
                    true,
                    effects);
        } else if (symbol == NormalizeEffect::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects", "Normalize"),
                    muse::mtrc("effects", "Sets the peak amplitude of a one or more tracks"),
                    false,
                    effects);
        } else if (symbol == FadeInEffect::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects", "Fade in"),
                    muse::mtrc("effects", "Applies a linear fade-in to the selected audio"),
                    true,
                    effects);
        } else if (symbol == FadeOutEffect::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects", "Fade out"),
                    muse::mtrc("effects", "Applies a linear fade-out to the selected audio"),
                    true,
                    effects);
        } else if (symbol == InvertEffect::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects", "Invert"),
                    muse::mtrc("effects", "Flips the audio samples upside-down, reversing their polarity"),
                    true,
                    effects);
        } else if (symbol == Repair::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects", "Repair"),
                    muse::mtrc("effects", "Sets the peak amplitude of a one or more tracks"),
                    false,
                    effects);
        } else if (symbol == ReverseEffect::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects", "Reverse"),
                    muse::mtrc("effects", "Reverses the selected audio"),
                    true,
                    effects);
        } else if (symbol == TruncateSilenceEffect::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects", "Truncate silence"),
                    muse::mtrc("effects",
                               "Automatically reduces the length of passages where the volume is below a specified level"),
                    true,
                    effects);
        }
#if USE_SOUNDTOUCH
        else if (symbol == ChangePitchEffect::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects", "Change pitch"),
                    muse::mtrc("effects", "Changes the pitch of a track without changing its tempo"),
                    true,
                    effects);
        }
#endif
        else if (symbol == ChirpEffect::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects", "Chirp"),
                    muse::mtrc("effects", "Generates an ascending or descending tone of one of four types"),
                    false,
                    effects);
        } else if (symbol == ToneEffect::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects", "Tone"),
                    muse::mtrc("effects", "Generates a constant frequency tone of one of four types"),
                    false,
                    effects);
        } else if (symbol == ReverbEffect::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects", "Reverb"),
                    muse::mtrc("effects", "Reverb effect"),
                    true,
                    effects);
        } else if (symbol == NoiseGenerator::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects/noise", "Noise"),
                    muse::mtrc("effects/noise", "Generates noise"),
                    false,
                    effects);
        } else if (symbol == NoiseReductionEffect::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects/noisereduction", "Noise reduction"),
                    muse::mtrc("effects/noisereduction", "Reduces noise in the audio"),
                    false,
                    effects);
        } else if (symbol == DtmfGenerator::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects/dtmf", "DTMF tones"),
                    muse::mtrc("effects/dtmf", "Generates DTMF signal"),
                    false,
                    effects);
        } else if (symbol == SilenceGenerator::Symbol) {
            addMeta(desc,
                    muse::mtrc("effects/silence", "Silence"),
                    muse::mtrc("effects/silence", "Generates silence"),
                    false,
                    effects);
        }
    }

    return effects;
}
