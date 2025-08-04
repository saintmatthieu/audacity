import QtQuick
import QtQuick.Layouts
import Muse.UiComponents
import Audacity.Effects

import "../common"

EffectBase {
    id: root

    property string title: qsTrc("effects/compressor", "Compressor")
    property bool isApplyAllowed: true

    width: 400
    implicitHeight: 400

    model: compressor

    CompressorViewModel {
        id: compressor

        instanceId: root.instanceId
    }

    Component.onCompleted: {
        compressor.init()
    }

    Column {

        ParameterKnob {
            CompressorSettingModel {
                id: thresholdDb
                paramId: "thresholdDb"
                instanceId: root.instanceId
            }
            parameter: {
                "title": qsTrc("effects/compressor", "thresholdDb"),
                "unit": "dB",
                "min": thresholdDb.min,
                "max": thresholdDb.max,
                "value": thresholdDb.value,
                "stepSize": thresholdDb.step
            }

            onNewValueRequested: function(_, newValue) {
                thresholdDb.value = newValue
            }

            onCommitRequested: {
                thresholdDb.commitSettings()
            }
        }

        ParameterKnob {
            CompressorSettingModel {
                id: makeupGainDb
                paramId: "makeupGainDb"
                instanceId: root.instanceId
            }
            parameter: {
                "title": qsTrc("effects/compressor", "makeupGainDb"),
                "unit": "dB",
                "min": makeupGainDb.min,
                "max": makeupGainDb.max,
                "value": makeupGainDb.value,
                "stepSize": makeupGainDb.step
            }

            onNewValueRequested: function(_, newValue) {
                makeupGainDb.value = newValue
            }

            onCommitRequested: {
                makeupGainDb.commitSettings()
            }
        }

        ParameterKnob {
            CompressorSettingModel {
                id: kneeWidthDb
                paramId: "kneeWidthDb"
                instanceId: root.instanceId
            }
            parameter: {
                "title": qsTrc("effects/compressor", "kneeWidthDb"),
                "unit": "dB",
                "min": kneeWidthDb.min,
                "max": kneeWidthDb.max,
                "value": kneeWidthDb.value,
                "stepSize": kneeWidthDb.step
            }

            onNewValueRequested: function(_, newValue) {
                kneeWidthDb.value = newValue
            }

            onCommitRequested: {
                kneeWidthDb.commitSettings()
            }
        }

        ParameterKnob {
            CompressorSettingModel {
                id: compressionRatio
                paramId: "compressionRatio"
                instanceId: root.instanceId
            }
            parameter: {
                "title": qsTrc("effects/compressor", "compressionRatio"),
                "unit": "dB",
                "min": compressionRatio.min,
                "max": compressionRatio.max,
                "value": compressionRatio.value,
                "stepSize": compressionRatio.step
            }

            onNewValueRequested: function(_, newValue) {
                compressionRatio.value = newValue
            }

            onCommitRequested: {
                compressionRatio.commitSettings()
            }
        }

        ParameterKnob {
            CompressorSettingModel {
                id: lookaheadMs
                paramId: "lookaheadMs"
                instanceId: root.instanceId
            }
            parameter: {
                "title": qsTrc("effects/compressor", "lookaheadMs"),
                "unit": "dB",
                "min": lookaheadMs.min,
                "max": lookaheadMs.max,
                "value": lookaheadMs.value,
                "stepSize": lookaheadMs.step
            }

            onNewValueRequested: function(_, newValue) {
                lookaheadMs.value = newValue
            }

            onCommitRequested: {
                lookaheadMs.commitSettings()
            }
        }

        ParameterKnob {
            CompressorSettingModel {
                id: attackMs
                paramId: "attackMs"
                instanceId: root.instanceId
            }
            parameter: {
                "title": qsTrc("effects/compressor", "attackMs"),
                "unit": "dB",
                "min": attackMs.min,
                "max": attackMs.max,
                "value": attackMs.value,
                "stepSize": attackMs.step
            }

            onNewValueRequested: function(_, newValue) {
                attackMs.value = newValue
            }

            onCommitRequested: {
                attackMs.commitSettings()
            }
        }

        ParameterKnob {
            CompressorSettingModel {
                id: releaseMs
                paramId: "releaseMs"
                instanceId: root.instanceId
            }
            parameter: {
                "title": qsTrc("effects/compressor", "releaseMs"),
                "unit": "dB",
                "min": releaseMs.min,
                "max": releaseMs.max,
                "value": releaseMs.value,
                "stepSize": releaseMs.step
            }

            onNewValueRequested: function(_, newValue) {
                releaseMs.value = newValue
            }

            onCommitRequested: {
                releaseMs.commitSettings()
            }
        }

        CheckBox {
            CompressorSettingModel {
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

        CheckBox {
            CompressorSettingModel {
                id: showOutput
                paramId: "showOutput"
                instanceId: root.instanceId
            }

            checked: showOutput.value
            onClicked: {
                showOutput.value = !checked
                showInput.commitSettings()
            }
        }

        CheckBox {
            CompressorSettingModel {
                id: showActual
                paramId: "showActual"
                instanceId: root.instanceId
            }

            checked: showActual.value
            onClicked: {
                showActual.value = !checked
                showInput.commitSettings()
            }
        }

        CheckBox {
            CompressorSettingModel {
                id: showTarget
                paramId: "showTarget"
                instanceId: root.instanceId
            }

            checked: showTarget.value
            onClicked: {
                showTarget.value = !checked
                showInput.commitSettings()
            }
        }


    }
}
