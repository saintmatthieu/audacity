/*
* Audacity: A Digital Audio Editor
*/
#include "effectsactionscontroller.h"
#include "au3audio/audioenginetypes.h"

#include "log.h"

using namespace au::effects;

static const muse::actions::ActionCode EFFECT_OPEN_CODE = "effect-open";

void EffectsActionsController::init()
{
    dispatcher()->reg(this, EFFECT_OPEN_CODE, this, &EffectsActionsController::doEffect);
    dispatcher()->reg(this, "repeat-last-effect", this, &EffectsActionsController::repeatLastEffect);
    dispatcher()->reg(this, "realtimeeffect-add", this, &EffectsActionsController::addRealtimeEffect);
    dispatcher()->reg(this, "realtimeeffect-remove", this, &EffectsActionsController::removeRealtimeEffect);
    dispatcher()->reg(this, "realtimeeffect-replace", this, &EffectsActionsController::replaceRealtimeEffect);

    effectExecutionScenario()->lastProcessorIsNowAvailable().onNotify(this, [this] {
        m_canReceiveActionsChanged.send({ "repeat-last-effect" });
    });
}

void EffectsActionsController::doEffect(const muse::actions::ActionData& args)
{
    IF_ASSERT_FAILED(args.count() > 0) {
        return;
    }

    muse::String effectId = args.arg<muse::String>(0);

    const auto ret = effectExecutionScenario()->performEffect(effectId);
    if (!ret && muse::Ret::Code(ret.code()) != muse::Ret::Code::Cancel) {
        const auto& title = effectsProvider()->meta(effectId).title;
        interactive()->error(title.toStdString(), ret.text());
    }
}

void EffectsActionsController::repeatLastEffect()
{
    effectExecutionScenario()->repeatLastProcessor();
}

void EffectsActionsController::addRealtimeEffect(const muse::actions::ActionData& args)
{
    IF_ASSERT_FAILED(args.count() == 2) {
        return;
    }

    const auto project = globalContext()->currentProject();
    IF_ASSERT_FAILED(project) {
        // Command issued without an open project ?..
        return;
    }

    const auto effectId = args.arg<au::audio::EffectId>(0);
    const auto trackId = args.arg<au::audio::TrackId>(1);
    // audioEngine()->addRealtimeEffect(*project, trackId, effectId);
    const auto source = project->trackeditProject()->trackAsAudioSource(trackId);
    audioEngine()->appendRealtimeEffect(source.get(), effectId.toStdString());
}

void EffectsActionsController::removeRealtimeEffect(const muse::actions::ActionData& args)
{
    IF_ASSERT_FAILED(args.count() == 2) {
        return;
    }

    const auto project = globalContext()->currentProject();
    IF_ASSERT_FAILED(project) {
        // Command issued without an open project ?..
        return;
    }

    const auto trackId = args.arg<au::audio::TrackId>(0);
    const auto effectState = args.arg<au::audio::EffectState>(1);
    audioEngine()->removeRealtimeEffect(*project, trackId, effectState);
}

void EffectsActionsController::replaceRealtimeEffect(const muse::actions::ActionData& args)
{
    IF_ASSERT_FAILED(args.count() == 3) {
        return;
    }

    const auto project = globalContext()->currentProject();
    IF_ASSERT_FAILED(project) {
        // Command issued without an open project ?..
        return;
    }

    const auto trackId = args.arg<au::audio::TrackId>(0);
    const auto srcIndex = args.arg<int>(1);
    const auto dstEffectId = args.arg<au::audio::EffectId>(2);

    audioEngine()->replaceRealtimeEffect(*project, trackId, srcIndex, dstEffectId);
}

bool EffectsActionsController::canReceiveAction(const muse::actions::ActionCode& code) const
{
    if (code == "repeat-last-effect") {
        return effectExecutionScenario()->lastProcessorIsAvailable();
    } else {
        return true;
    }
}

muse::async::Channel<muse::actions::ActionCodeList> EffectsActionsController::canReceiveActionsChanged() const
{
    return m_canReceiveActionsChanged;
}
