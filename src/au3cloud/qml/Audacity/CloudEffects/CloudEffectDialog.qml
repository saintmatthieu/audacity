/*
* Audacity: A Digital Audio Editor
*/
import QtQuick

import Muse.Ui
import Muse.UiComponents

//! NOTE Generic host dialog for a shipped cloud effect. Loads the CloudEffect-derived
//! component given by `effectUrl` (passed as a uri query param). The loaded CloudEffect
//! closes this window itself (Window.window.close()) on Cancel / Apply.
StyledDialogView {
    id: dialog

    objectName: "CloudEffectDialog"

    //! NOTE Set from the open uri: ...?effectUrl=<qrc url of the effect qml>
    property string effectUrl: ""

    contentWidth: loader.implicitWidth > 0 ? loader.implicitWidth : 480
    contentHeight: loader.implicitHeight > 0 ? loader.implicitHeight : 360

    margins: 0

    Loader {
        id: loader

        anchors.fill: parent

        source: dialog.effectUrl
    }
}
