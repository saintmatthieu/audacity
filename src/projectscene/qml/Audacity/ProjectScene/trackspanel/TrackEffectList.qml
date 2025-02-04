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
    implicitHeight: trackEffectList.contentHeight
    readonly property int topMargin: 8
    property alias isMasterTrack: trackEffectListModel.isMasterTrack
    property alias trackName: trackEffectListModel.trackName
    property alias trackEffectsActive: trackEffectListModel.trackEffectsActive
    property bool empty: trackEffectList.count == 0

    Component.onCompleted: {
        trackEffectListModel.load()
    }

    RealtimeEffectListModel {
        id: trackEffectListModel
    }

    Component {
        id: listMargin
        Item {
            height: root.topMargin
        }
    }

    StyledListView {
        id: trackEffectList
        anchors.fill: parent
        spacing: 6
        cacheBuffer: 3000
        interactive: true
        model: trackEffectListModel
        boundsBehavior: Flickable.DragAndOvershootBounds
        boundsMovement: Flickable.FollowBoundsBehavior
        flickDeceleration: 10000
        footer: listMargin
        header: listMargin

        clip: true
        anchors.margins: 0

        delegate: RealtimeEffectListItem {
            item: itemData
            listView: trackEffectList
            availableEffects: trackEffectList.model.availableEffects
            handleMenuItemWithState: trackEffectList.model.handleMenuItemWithState
            width: trackEffectList.width - scrollbarContainer.width
        }

        ScrollBar.vertical: scrollbar

        Item {
            id: scrollbarContainer
            width: 12
            height: parent.height
            anchors.right: parent.right

            StyledScrollBar {
                id: scrollbar
                anchors.fill: parent
                thickness: 5
                policy: ScrollBar.AlwaysOn
            }
        }
    }
}
