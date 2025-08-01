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
        KnobControl {
            LimiterSettingModel {
                id: threshold
                paramId: "thresholdDb"
                instanceId: root.instanceId
            }

            currentValue: threshold.value
            from: threshold.min
            to: threshold.max
            stepSize: threshold.step
            onNewValueRequested: function(newValue) {
                threshold.value = newValue
            }
            onMouseReleased: {
                threshold.commitSettings()
            }
        }

        KnobControl {
            LimiterSettingModel {
                id: makeup
                paramId: "makeupTargetDb"
                instanceId: root.instanceId
            }

            currentValue: makeup.value
            from: makeup.min
            to: makeup.max
            stepSize: makeup.step
            onNewValueRequested: function(newValue) {
                makeup.value = newValue
            }
            onMouseReleased: {
                makeup.commitSettings()
            }
        }

            LimiterSettingModel {
                id: lookahead
                paramId: "lookaheadMs"
                instanceId: root.instanceId
            }

            currentValue: knee.value
            from: knee.min
            to: knee.max
            stepSize: knee.step
            onNewValueRequested: function(newValue) {
                knee.value = newValue
            }
            onMouseReleased: {
                knee.commitSettings()
            }
        }

            LimiterSettingModel {
                id: knee
                paramId: "kneeWidthDb"
                instanceId: root.instanceId
            }

            currentValue: lookahead.value
            from: lookahead.min
            to: lookahead.max
            stepSize: lookahead.step
            onNewValueRequested: function(newValue) {
                lookahead.value = newValue
            }
            onMouseReleased: {
                lookahead.commitSettings()
            }
        }

        KnobControl {
            LimiterSettingModel {
                id: release
                paramId: "releaseMs"
                instanceId: root.instanceId
            }

            currentValue: release.value
            from: release.min
            to: release.max
            stepSize: release.step
            onNewValueRequested: function(newValue) {
                release.value = newValue
            }
            onMouseReleased: {
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
