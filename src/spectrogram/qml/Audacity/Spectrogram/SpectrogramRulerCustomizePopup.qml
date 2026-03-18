/*
 * Audacity: A Digital Audio Editor
 */
import QtQuick 2.15

import Muse.UiComponents
import Muse.Ui 1.0

import Audacity.Preferences 1.0 // TODO move to common
import Audacity.Playback 1.0 // TODO move to common
import Audacity.Spectrogram 1.0

StyledPopupView {
    id: root

    required property SpectrogramChannelRulerModel rulerModel

    contentWidth: prv.popupWidth - 2 * prv.popupMargins
    contentHeight: prv.popupHeight - 2 * prv.popupMargins

    margins: prv.popupMargins
    placementPolicies: PopupView.PreferLeft

    property alias settingsModel: settingsModel
    property alias minMaxNavigationPanel: minMaxNavigationPanel

    TrackSpectrogramSettingsModel {
        id: settingsModel
        trackId: root.rulerModel.trackId
    }

    QtObject {
        id: prv

        readonly property int popupWidth: 240
        readonly property int popupHeight: 370

        readonly property int popupMargins: 12
        readonly property int itemsSpacing: 12
        readonly property int btnSpacing: 6

        readonly property int btnHeight: 28
        readonly property int zoomBtnWidth: 40
        readonly property int resetBtnWidth: 85
        readonly property int formatGroupBoxHeight: 180

        property int prefsColumnWidth: 68
        readonly property int prefsColumnSpacing: 8

        readonly property int smallControlWidth: prefsColumnWidth
        readonly property int mediumControlWidth: 2 * prefsColumnWidth + prefsColumnSpacing
        readonly property int largeControlWidth: 3 * prefsColumnWidth + 2 * prefsColumnSpacing

        readonly property int narrowSpacing: 8
        readonly property int mediumSpacing: 16
    }

    Column {
        anchors.fill: parent
        spacing: prv.itemsSpacing

        Row {
            width: parent.width
            height: prv.btnHeight

            spacing: prv.btnSpacing

            NavigationPanel {
                id: zoomNavigationPanel
                section: root.navigationSection
                name: "ZoomNavigationPanel"
                order: 0
            }

            FlatButton {
                id: zoomInBtn

                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: prv.zoomBtnWidth

                navigation.panel: zoomNavigationPanel
                navigation.order: 0

                normalColor: ui.theme.buttonColor
                icon: IconCode.ZOOM_IN

                onClicked: {
                    rulerModel.zoomInFromPopup()
                }
            }

            FlatButton {
                id: zoomOutBtn

                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: prv.zoomBtnWidth

                navigation.panel: zoomNavigationPanel
                navigation.order: zoomInBtn.navigation.order + 1

                normalColor: ui.theme.buttonColor
                icon: IconCode.ZOOM_OUT

                enabled: !rulerModel.isMinZoom

                onClicked: {
                    rulerModel.zoomOutFromPopup()
                }
            }

            FlatButton {
                id: resetBtn

                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: prv.resetBtnWidth

                navigation.panel: zoomNavigationPanel
                navigation.order: zoomOutBtn.navigation.order + 1

                normalColor: ui.theme.buttonColor
                icon: IconCode.UNDO

                orientation: Qt.Horizontal

                text: qsTrc("spectrogram", "Reset")

                enabled: !rulerModel.isMinZoom

                onClicked: {
                    rulerModel.resetZoom()
                }
            }
        }

        StyledGroupBox {
            id: scaleGroupBox

            width: parent.width
            height: prv.formatGroupBoxHeight

            title: qsTrc("spectrogram", "Scale")

            navPanel.section: root.navigationSection
            navPanel.order: 1
            navPanel.name: "ScaleGroupBox"

            titleSpacing: 4

            backgroundColor: ui.theme.backgroundSecondaryColor

            model: {
                const result = []
                for (var i = 0; i < settingsModel.scaleNames.length; i++) {
                    result.push({
                        label: settingsModel.scaleNames[i],
                        value: i
                    })
                }
                return result
            }
            value: settingsModel.scale

            onValueChangeRequested: function (newValue) {
                settingsModel.scale = newValue
            }
        }

        Column {
            width: parent.width
            spacing: 4

            StyledTextLabel {
                width: parent.width
                text: qsTrc("spectrogram", "Frequency range")
                horizontalAlignment: Text.AlignLeft
            }

            NavigationPanel {
                id: minMaxNavigationPanel
                section: root.navigationSection
                name: "MinMaxNavigationPanel"
                order: 2
            }

            Repeater {
                id: repeater

                model: ScaleSectionParameterListModel {
                    settingsModel: root.settingsModel
                    trackId: root.rulerModel.trackId
                    columnWidth: prv.prefsColumnWidth
                }

                Row {
                    width: parent.width
                    height: control.implicitHeight

                    StyledTextLabel {
                        width: 32
                        anchors.verticalCenter: parent.verticalCenter

                        text: shortControlLabel
                        horizontalAlignment: Text.AlignLeft
                    }

                    IncrementalPropertyControl {
                        id: control

                        width: prv.smallControlWidth
                        anchors.verticalCenter: parent.verticalCenter

                        navigation.panel: root.minMaxNavigationPanel
                        navigation.order: index

                        minValue: controlMinValue
                        maxValue: controlMaxValue
                        measureUnitsSymbol: controlUnits
                        decimals: 0
                        step: 1

                        currentValue: controlCurrentValue
                        onValueEditingFinished: function (value) {
                            controlCurrentValue = value
                        }
                    }
                }
            }
        }
    }
}
