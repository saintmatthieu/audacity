/*
* Audacity: A Digital Audio Editor
*/
import QtQuick
import QtQuick.Layouts

import Muse.Ui
import Muse.UiComponents

import Audacity.Vst

StyledDialogView {
    id: root

    property var instanceId
    property bool isVst: false

    property alias viewer: loader.item
    property bool isApplyAllowed: isVst || (viewer && viewer.isApplyAllowed)

    Component.onCompleted: {
        loader.sourceComponent = isVst ? vstViewer : builtinViewer
    }

    title: viewer ? viewer.title : ""

    contentWidth: Math.max(viewer ? viewer.implicitWidth : 0, bbox.implicitWidth)
    contentHeight: (viewer ? viewer.implicitHeight : 0) + bbox.implicitHeight + 16

    margins: 16

    Component {
        id: builtinViewer
        EffectsViewer {
            instanceId: root.instanceId
            width: parent.width
        }
    }

    Component {
        id: vstViewer
        VstViewer {
            instanceId: root.instanceId
            width: root.contentWidth
            height: implicitHeight
        }
    }

    Loader {
        id: loader
        anchors.fill: parent
    }

    ButtonBox {
        id: bbox
        width: parent.width
        anchors.bottom: parent.bottom

        //! TODO Move function to ButtonBox (Muse framework)
        function buttonById(id) {
            for (var i = 0; i < bbox.count; i++) {
                var btn = bbox.itemAt(i)
                if (btn.buttonId === id) {
                    return btn
                }
            }

            return null
        }

        FlatButton {
            id: manageBtn
            text: qsTrc("effects", "Manage")
            buttonRole: ButtonBoxModel.CustomRole
            buttonId: ButtonBoxModel.CustomButton + 1
            isLeftSide: true
            onClicked: viewer.manage(manageBtn)
        }

        FlatButton {
            id: previewBtn
            text: qsTrc("effects", "Preview")
            buttonRole: ButtonBoxModel.CustomRole
            buttonId: ButtonBoxModel.CustomButton + 2
            isLeftSide: true
            onClicked: viewer.preview()
            enabled: root.isApplyAllowed
        }

        FlatButton {
            text: qsTrc("global", "Cancel")
            buttonRole: ButtonBoxModel.RejectRole
            buttonId: ButtonBoxModel.Cancel
            onClicked: root.reject()
        }

        FlatButton {
            text: qsTrc("global", "Apply")
            buttonRole: ButtonBoxModel.AcceptRole
            buttonId: ButtonBoxModel.Apply
            accentButton: true
            onClicked: root.accept()
            enabled: root.isApplyAllowed
        }
    }
}
