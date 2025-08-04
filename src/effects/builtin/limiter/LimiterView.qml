import QtQuick
import QtQuick.Layouts
import Muse.UiComponents
import Audacity.Effects
import Audacity.ProjectScene

import "../common"

EffectBase {
    id: root

    property string title: qsTrc("effects/limiter", "Limiter")
    property bool isApplyAllowed: true

    width: 400
    implicitHeight: 400

    model: limiter

    LimiterViewModel {
        id: limiter

        instanceId: root.instanceId
    }

    Component.onCompleted: {
        limiter.init()
        threshold.init()
        makeup.init()
        knee.init()
        lookahead.init()
        release.init()
        showInput.init()
    }

    Column {
        BigParameterKnob {
            LimiterSettingModel {
                id: threshold
                paramId: "thresholdDb"
                instanceId: root.instanceId
            }

            parameter: {
                "title": qsTrc("effects/limiter", "Threshold"),
                "unit": "dB",
                "min": threshold.min,
                "max": threshold.max,
                "value": threshold.value,
                "stepSize": threshold.step
            }

            onNewValueRequested: function(_, newValue) {
                threshold.value = newValue
            }

            onCommitRequested: {
                threshold.commitSettings()
            }
        }

        BigParameterKnob {
            LimiterSettingModel {
                id: makeup
                paramId: "makeupTargetDb"
                instanceId: root.instanceId
            }

            parameter: {
                "title": qsTrc("effects/limiter", "Makeup"),
                "unit": "dB",
                "min": makeup.min,
                "max": makeup.max,
                "value": makeup.value,
                "stepSize": makeup.step
            }

            onNewValueRequested: function(_, newValue) {
                makeup.value = newValue
            }

            onCommitRequested: {
                makeup.commitSettings()
            }
        }

        ParameterKnob {
            LimiterSettingModel {
                id: lookahead
                paramId: "lookaheadMs"
                instanceId: root.instanceId
            }

            parameter: {
                "title": qsTrc("effects/limiter", "Lookahead"),
                "unit": "ms", // TODO should come from model
                "min": lookahead.min,
                "max": lookahead.max,
                "value": lookahead.value,
                "stepSize": lookahead.step
            }

            onNewValueRequested: function(_, newValue) {
                lookahead.value = newValue
            }

            onCommitRequested: {
                lookahead.commitSettings()
            }
        }

        ParameterKnob {
            LimiterSettingModel {
                id: knee
                paramId: "kneeWidthDb"
                instanceId: root.instanceId
            }

            parameter: {
                "title": qsTrc("effects/limiter", "Knee width"),
                "unit": "dB",
                "min": knee.min,
                "max": knee.max,
                "value": knee.value,
                "stepSize": knee.step
            }

            onNewValueRequested: function(_, newValue) {
                knee.value = newValue
            }

            onCommitRequested: {
                knee.commitSettings()
            }
        }

        ParameterKnob {
            LimiterSettingModel {
                id: release
                paramId: "releaseMs"
                instanceId: root.instanceId
            }

            parameter: {
                "title": qsTrc("effects/limiter", "Release"),
                "unit": "ms",
                "min": release.min,
                "max": release.max,
                "value": release.value,
                "stepSize": release.step
            }

            onNewValueRequested: function(_, newValue) {
                release.value = newValue
            }

            onCommitRequested: {
                release.commitSettings()
            }
        }

        CheckBox {
            LimiterSettingModel {
                id: showInput
                paramId: "showInput"
                instanceId: root.instanceId
            }

            checked: showInput.value
            onClicked: {
                showInput.value = !checked
                showInput.commitSettings()
            }
        }
    }
}
