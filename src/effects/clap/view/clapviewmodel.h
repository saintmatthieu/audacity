/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include "modularity/ioc.h"
#include "effects/effects_base/ieffectinstancesregister.h"
#include "effects/effects_base/ieffectexecutionscenario.h"
#include "effects/effects_base/view/abstracteffectviewmodel.h"

namespace au::effects {
//! Minimal view model for the native CLAP viewer. The CLAP plugin owns its own
//! parameter UI internally, so the model only needs preview start/stop plumbing
//! and the instanceId routing the QML expects.
class ClapViewModel : public AbstractEffectViewModel
{
    Q_OBJECT

public:
    muse::GlobalInject<IEffectInstancesRegister> instancesRegister;
    muse::ContextInject<IEffectExecutionScenario> executionScenario{ this };

    ClapViewModel(QObject* parent, int instanceId);
    ~ClapViewModel() override = default;

private:
    void doInit() override;
    void doStartPreview() override;
    void doStopPreview() override;
};

class ClapViewModelFactory : public EffectViewModelFactory<ClapViewModel>
{
};
}
