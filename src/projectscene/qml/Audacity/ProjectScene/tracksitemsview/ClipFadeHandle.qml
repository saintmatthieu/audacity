import QtQuick

import Muse.Ui

Item {
    id: root

    required property bool isFadeIn
    required property real fadeWidth

    readonly property alias containsMouse: mouseArea.containsMouse
    readonly property alias pressed: mouseArea.pressed

    signal fadeWidthChangeRequested(real width, bool completed)

    // in parent coordinates
    signal mousePositionChanged(real x, real y)

    ClipHandleIcon {
        anchors.centerIn: parent

        hovered: mouseArea.containsMouse || mouseArea.pressed
        iconCode: root.isFadeIn ? IconCode.FADE_HANDLE_IN : IconCode.FADE_HANDLE_OUT
        color: ui.theme.extra["black_color"]
        style: Text.Outline
        styleColor: ui.theme.extra["white_color"]
    }

    MouseArea {
        id: mouseArea

        property real pressX: 0
        property real pressFadeWidth: 0

        anchors.fill: parent

        hoverEnabled: true
        cursorShape: Qt.SizeHorCursor

        onPressed: function (e) {
            // in parent coordinates: the handle itself moves while dragging
            pressX = mapToItem(root.parent, e.x, e.y).x
            pressFadeWidth = root.fadeWidth
        }

        function requestFadeWidth(e, completed) {
            const dx = mapToItem(root.parent, e.x, e.y).x - pressX
            root.fadeWidthChangeRequested(pressFadeWidth + (root.isFadeIn ? dx : -dx), completed)
        }

        onPositionChanged: function (e) {
            const pos = mapToItem(root.parent, e.x, e.y)
            root.mousePositionChanged(pos.x, pos.y)

            if (pressed) {
                requestFadeWidth(e, false)
            }
        }

        onReleased: function (e) {
            requestFadeWidth(e, true)
        }
    }
}
