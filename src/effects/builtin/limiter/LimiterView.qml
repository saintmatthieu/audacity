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

    width: knobRow.width
    implicitHeight: knobRow.height

    model: limiter

    LimiterViewModel {
        id: limiter

        instanceId: root.instanceId
    }

    Component.onCompleted: {
        limiter.init()
    }

    Rectangle {

        color: ui.theme.backgroundSecondaryColor
        border.color: ui.theme.strokeColor
        width: knobRow.width
        height: knobRow.height
        radius: 4

        Row {
            id: knobRow

            spacing: 16
            padding: 16

            Column {
                id: thresholdColumn

                topPadding: 12
                bottomPadding: 12

                SettingKnob {
                    id: thresholdKnob

                    isBig: true
                    title: qsTrc("effects/limiter", "Threshold")
                    unit: "dB"
                    model: LimiterSettingModel {
                        paramId: "thresholdDb"
                        instanceId: root.instanceId
                    }
                }
            }

            Column {
                topPadding: 12
                bottomPadding: 12

                SettingKnob {
                    id: makeupKnob

                    isBig: true
                    title: qsTrc("effects/limiter", "Make-up gain")
                    unit: "dB"
                    model: LimiterSettingModel {
                        paramId: "makeupTargetDb"
                        instanceId: root.instanceId
                    }
                }
            }

            // Can't use a SeparatorLine in a Row or Column, or we get an infinite loop.
            Rectangle {
                width: 1
                height: thresholdColumn.height
                color: ui.theme.strokeColor
            }

            SettingKnob {
                id: lookaheadKnob

                title: qsTrc("effects/limiter", "Lookahead")
                unit: "ms"
                model: LimiterSettingModel {
                    paramId: "lookaheadMs"
                    instanceId: root.instanceId
                }
            }

            SettingKnob {
                id: kneeKnob

                title: qsTrc("effects/limiter", "Knee width")
                unit: "dB"
                model: LimiterSettingModel {
                    paramId: "kneeWidthDb"
                    instanceId: root.instanceId
                }
            }

            SettingKnob {
                id: releaseKnob

                title: qsTrc("effects/limiter", "Release")
                unit: "ms"
                model: LimiterSettingModel {
                    paramId: "releaseMs"
                    instanceId: root.instanceId
                }
            }
        }
    }
}
