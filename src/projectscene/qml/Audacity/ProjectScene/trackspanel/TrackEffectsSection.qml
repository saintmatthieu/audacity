/*
 * Audacity: A Digital Audio Editor
 */
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import Muse.Ui
import Muse.UiComponents

import Audacity.ProjectScene

Rectangle {
    id: root

    property int selectedTrackIndex: -1
    property alias trackList: trackList

    color: ui.theme.backgroundPrimaryColor

    onSelectedTrackIndexChanged: {
        const selectedTrackItem = view.itemAtIndex(selectedTrackIndex)
        trackEffects.trackName = selectedTrackItem.item.title
    }

    TracksListModel {
        id: trackList
    }

    ColumnLayout {
        id: trackEffects

        property alias trackName: trackEffectsHeader.trackName
        readonly property int itemSpacing: 12

        spacing: 0
        width: parent.width

        SeparatorLine { }

        RowLayout {
            id: trackEffectsHeader
            property alias trackName: trackNameLabel.text
            readonly property int headerHeight: 40

            // visible: !root.trackList.isEmpty
            spacing: trackEffects.itemSpacing

            Layout.fillWidth: true
            Layout.preferredHeight: headerHeight

            FlatButton {
                id: trackEffectsPowerButton

                Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft
                Layout.margins: 8
                Layout.preferredWidth: trackEffectsHeader.headerHeight - Layout.margins * 2
                Layout.preferredHeight: Layout.preferredWidth

                icon: IconCode.BYPASS
                iconFont: ui.theme.toolbarIconsFont

                accentButton: true

                onClicked: {
                    accentButton = !accentButton
                }
            }

            StyledTextLabel {
                id: trackNameLabel
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.maximumWidth: root.width - trackEffectsPowerButton.width - trackEffectsHeader.spacing - trackEffects.itemSpacing
                horizontalAlignment: Text.AlignLeft
            }
        }

        SeparatorLine {
            id: trackEffectsBottom
            width: effectsSectionWidth
            // visible: !root.trackList.isEmpty
        }

        Rectangle {
            color: ui.theme.backgroundSecondaryColor
            Layout.preferredHeight: effectList.preferredHeight == 0 ? 0 : effectList.preferredHeight + (effectList.anchors.topMargin + effectList.anchors.bottomMargin)
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.fillWidth: true
            TrackEffectList {
                id: effectList
                color: "transparent"
                anchors {
                    top: parent.top
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                    leftMargin: 4
                    rightMargin: 12
                    topMargin: 8
                    bottomMargin: 8
                }
            }
        }

        SeparatorLine {
            visible: effectList.visible
        }

        FlatButton {
            Layout.fillWidth: true
            Layout.preferredHeight: 24
            Layout.margins: trackEffects.itemSpacing
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVTop

            // enabled: !root.trackList.isEmpty
            text: qsTrc("projectscene", "Add effect")

            RealtimeEffectMenuModel {
                id: menuModel
                trackId: view.itemAtIndex(root.selectedTrackIndex).item.trackId
            }

            onClicked: function() {
                menuModel.load()
                effectMenuLoader.toggleOpened(menuModel)
            }

            StyledMenuLoader {
                id: effectMenuLoader

                onHandleMenuItem: function(itemId) {
                    menuModel.handleMenuItem(itemId)
                }
            }
        }
    }
}
