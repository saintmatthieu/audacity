import QtQuick
import QtQuick.Layouts

import Muse.Ui
import Muse.UiComponents

import Audacity.BuiltinEffects

Rectangle {

    id: root

    property AbstractEffectModel model: null

    signal inited()

    color: ui.theme.backgroundPrimaryColor

    function preview() {
        root.model.preview()
    }
}
