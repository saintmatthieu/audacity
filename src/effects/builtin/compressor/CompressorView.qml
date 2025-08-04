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
        Repeater {
            model: [
                { id: "attackMs", title: qsTrc("effects/compressor", "Attack"), unit: "ms" },
                { id: "releaseMs", title: qsTrc("effects/compressor", "Release"), unit: "ms" },
                { id: "lookaheadMs", title: qsTrc("effects/compressor", "Lookahead"), unit: "ms" },
                { id: "thresholdDb", title: qsTrc("effects/compressor", "Threshold"), unit: "dB" },
                { id: "compressionRatio", title: qsTrc("effects/compressor", "Ratio"), unit: "" },
                { id: "kneeWidthDb", title: qsTrc("effects/compressor", "Knee width"), unit: "dB" },
                { id: "makeupGainDb", title: qsTrc("effects/compressor", "Make-up gain"), unit: "dB" }
            ]

            delegate: SettingKnob {
                required property var modelData
                title: modelData.title
                unit: modelData.unit
                model: CompressorSettingModel {
                    paramId: modelData.id
                    instanceId: root.instanceId
                }
            }
        }
    }
}
