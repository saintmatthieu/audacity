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
    property int scrollOffset: 0
    property int topMargin: 0
    readonly property int animationDuration: 200
    property int itemHeight: listView ? height + listView.spacing : 0

    height: 24
    y: index * itemHeight + yOffset
    clip: false // should be true?
    background.color: "transparent"
    hoverHitColor: "transparent"

    Behavior on y {
        NumberAnimation {
            id: yAnimation
            duration: animationDuration
            easing.type: Easing.InOutQuad
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
                const posInListView = content.mapToItem(listView, 0, itemHeight / 2 - topMargin).y + scrollOffset
                const targetIndex = Math.floor(posInListView / itemHeight)
                const siblings = listView.contentItem.children

                const prevContentY = listView.contentY
                // Temporarily disable the animation, otherwise the dragged item first returns to its original position
                // before sliding to its new position, which looks strange.
                yAnimation.duration = 0
                for (var i = 0; i < siblings.length; ++i) {
                    const sibling = siblings[i]
                    if (sibling.hasOwnProperty("yOffset")) {
                        sibling.yOffset = 0
                    }
                }
                listView.model.moveRow(root.index, targetIndex)
                listView.contentY = prevContentY
                yAnimation.duration = animationDuration
            }
            mouseArea.onPositionChanged: {
                if (!mouseArea.drag.active)
                    return
                const posInListView = content.mapToItem(listView, 0, itemHeight / 2 - topMargin).y + scrollOffset
                const targetIndex = Math.floor(posInListView / itemHeight)
                const siblings = listView.contentItem.children
                for (var i = 0; i < siblings.length; ++i) {
                    const sibling = siblings[i]
                    if (!sibling.hasOwnProperty("yOffset"))
                        continue
                    else if (sibling.index > root.index && sibling.index <= targetIndex) {
                        sibling.yOffset = -itemHeight
                    } else if (sibling.index < root.index && sibling.index >= targetIndex) {
                        sibling.yOffset = itemHeight
                    } else {
                        sibling.yOffset = 0
                    }
                }
                // Check if the mouse position is at the top or bottom of the list
                if (posInListView < 16 || posInListView > listView.height - 16) {
                    // Start a timer to scroll the list
                    if (!scrollTimer.running) {
                        console.log("Starting scroll timer")
                        scrollTimer.start()
                    }
                }
            }

            mouseArea.drag.minimumY: {
                const origHotspotY = (root.index + 0.5) * itemHeight
                return -origHotspotY - 5
            }
            mouseArea.drag.maximumY: {
                const origHotspotY = (root.index + 0.5) * itemHeight
                return listView.contentHeight - origHotspotY - 5
            }

            Timer {
                // log every second
                interval: 1000
                running: true
                repeat: true
                onTriggered: {
                    // console.log("contentY", listView.contentY + topMargin)
                }
            }

            Timer {
                id: scrollTimer
                // 60 fps
                interval: 1000 / 60
                repeat: true
                running: false
                onTriggered: {
                    const listY = listView.contentY + topMargin
                    const posInListView = content.mapToItem(listView, 0, itemHeight / 2 - topMargin).y + scrollOffset
                    const targetIndex = Math.floor(posInListView / itemHeight)
                    const siblings = listView.contentItem.children
                    if (posInListView < 0) {
                        const nextY = listView.contentY - 1
                        if (nextY <= 0) {
                            console.log("Reached top")
                            stop()
                            return
                        }
                        listView.contentY = nextY
                    } else if (posInListView > listView.height) {
                        const nextY = listView.contentY + 1
                        if (nextY >= listView.contentHeight - listView.height) {
                            console.log("Reached bottom")
                            stop()
                            return
                        }
                        listView.contentY = nextY
                    }
                }
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
