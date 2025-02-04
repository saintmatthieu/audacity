import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Muse.Ui
import Muse.UiComponents

import Audacity.Effects

ListItemBlank {
    id: root

    property var item: null
    property var listView: null
    property var availableEffects: null
    property var handleMenuItemWithState: null
    property int index: -1
    property int yOffset: 0
    onYOffsetChanged: console.log("yOffset:", yOffset)

    property int itemHeight: listView ? height + listView.spacing : 0
    property alias yOffsetAnimation: yOffsetAnimation

    readonly property int yOffsetAnimationDuration: 0

    height: 24
    y: index * itemHeight + yOffset
    clip: false // should be true?
    background.color: "transparent"
    hoverHitColor: "transparent"

    Behavior on yOffset {
       NumberAnimation {
           id: yOffsetAnimation
           duration: yOffsetAnimationDuration
           easing.type: Easing.InOutQuad
       }
    }

    DropArea {
        id: dropArea

        anchors.fill: parent
        enabled: !content.Drag.active

        onContainsDragChanged: {
            if (!dropArea.drag.source)
                return
            const draggedIndex = dropArea.drag.source.index
            const myIndex = root.index
            if (!containsDrag || yOffsetAnimation.running || draggedIndex === myIndex) {
                return
            }
            if (root.yOffset !== 0) {
                root.yOffset = 0
                console.log("index", myIndex, "offset reset to 0")
                return
            }
            if (draggedIndex < myIndex)
                root.yOffset = -itemHeight
            else
                root.yOffset = itemHeight
            console.log("index", myIndex, "offset:", root.yOffset)
        }
    }

    RowLayout {
        id: content

        property alias item: root.item
        property alias index: root.index

        width: parent.width
        height: parent.height
        anchors.verticalCenter: parent.verticalCenter
        anchors.horizontalCenter: parent.horizontalCenter

        Drag.active: gripButton.mouseArea.drag.active
        Drag.hotSpot.x: width / 2
        Drag.hotSpot.y: height / 2

        spacing: 4

        states: State {
            when: gripButton.mouseArea.drag.active
            AnchorChanges { target: content; anchors.verticalCenter: undefined }
            PropertyChanges { target: root; z: listView.model.count() }
        }

        FlatButton {
            id: gripButton

            Layout.preferredWidth: 14
            Layout.preferredHeight: 16
            Layout.alignment: Qt.AlignVCenter
            backgroundRadius: 0

            visible: true
            transparent: true
            hoverHitColor: ui.theme.buttonColor
            mouseArea.cursorShape: Qt.SizeAllCursor

            // do not accept buttons as FlatButton's mouseArea will override
            // DockTitleBar mouseArea and effect will not be draggable
            mouseArea.drag.target: content
            mouseArea.drag.axis: Drag.YAxis
            mouseArea.onReleased: {
                const items = listView.contentItem.children
                for (var i = 0; i < items.length; i++) {
                    var item = items[i]
                    if (item.hasOwnProperty("yOffset")) {
                        item.yOffsetAnimation.duration = 0
                        item.yOffset = 0
                        item.yOffsetAnimation.duration = yOffsetAnimationDuration
                    }
                }
                const posInListView = content.mapToItem(listView, 0, 0).y
                const targetIndex = Math.round(posInListView / itemHeight)
                listView.model.moveRow(root.index, targetIndex)
            }

            contentItem: StyledIconLabel {
                iconCode: IconCode.TOOLBAR_GRIP
            }

        }

        BypassEffectButton {
            Layout.margins: 0
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: root.height
            Layout.minimumHeight: root.height
            Layout.maximumHeight: root.height

            isMasterEffect: item && item.isMasterEffect
            accentButton: item && item.isActive

            onClicked: {
                item.isActive = item && !item.isActive
            }
        }

        // Wrappinng a FlatButton in a Rectangle because of two bad reasons:
        // * I don't find a `border` property for `FlatButton`
        // * Somehow, the button's color is a bit darkened it direct child of the RowLayout
        Rectangle {
            id: effectNameRect

            border.color: ui.theme.strokeColor
            border.width: 1
            radius: effectNameButton.backgroundRadius
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 148

            FlatButton {
                id: effectNameButton

                anchors.fill: parent
                normalColor: ui.theme.backgroundPrimaryColor

                StyledTextLabel {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    id: trackNameLabel
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    text: root.item ? root.item.effectName() : ""
                }

                onClicked: {
                    root.item.toggleDialog()
                }
            }
        }

        // Wrapping a FlatButton for the same reasons as above.
        Rectangle {
            id: chooseEffectRect

            border.color: ui.theme.strokeColor
            border.width: 1
            radius: effectNameButton.backgroundRadius
            Layout.fillHeight: true
            Layout.preferredWidth: height

            FlatButton {
                id: chooseEffectDropdown

                anchors.fill: parent
                normalColor: ui.theme.backgroundPrimaryColor
                icon: IconCode.SMALL_ARROW_DOWN

                onClicked: {
                    effectMenuLoader.toggleOpened(root.availableEffects)
                }

                StyledMenuLoader {
                    id: effectMenuLoader

                    onHandleMenuItem: function(menuItem) {
                        root.handleMenuItemWithState(menuItem, root.item)
                    }
                }
            }
        }
    }
}
