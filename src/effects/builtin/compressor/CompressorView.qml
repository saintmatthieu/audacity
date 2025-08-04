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

        SettingKnob {
            id: thresholdDbKnob

            title: qsTrc("effects/compressor", "thresholdDb")
            unit: "dB"
            model: CompressorSettingModel {
                paramId: "thresholdDb"
                instanceId: root.instanceId
            }
        }

        SettingKnob {
            id: makeupGainDbKnob

            title: qsTrc("effects/compressor", "makeupGainDb")
            unit: "dB"
            model: CompressorSettingModel {
                paramId: "makeupGainDb"
                instanceId: root.instanceId
            }
        }

        SettingKnob {
            id: kneeWidthDbKnob

            title: qsTrc("effects/compressor", "kneeWidthDb")
            unit: "dB"
            model: CompressorSettingModel {
                paramId: "kneeWidthDb"
                instanceId: root.instanceId
            }
        }

        SettingKnob {
            id: compressionRatioKnob

            title: qsTrc("effects/compressor", "compressionRatio")
            unit: "dB"
            model: CompressorSettingModel {
                paramId: "compressionRatio"
                instanceId: root.instanceId
            }
        }

        SettingKnob {
            id: lookaheadMsKnob

            title: qsTrc("effects/compressor", "lookaheadMs")
            unit: "dB"
            model: CompressorSettingModel {
                paramId: "lookaheadMs"
                instanceId: root.instanceId
            }
        }

        SettingKnob {
            id: attackMsKnob

            title: qsTrc("effects/compressor", "attackMs")
            unit: "dB"
            model: CompressorSettingModel {
                paramId: "attackMs"
                instanceId: root.instanceId
            }
        }

        SettingKnob {
            id: releaseMsKnob

            title: qsTrc("effects/compressor", "releaseMs")
            unit: "dB"
            model: CompressorSettingModel {
                paramId: "releaseMs"
                instanceId: root.instanceId
            }
        }
    }
}
