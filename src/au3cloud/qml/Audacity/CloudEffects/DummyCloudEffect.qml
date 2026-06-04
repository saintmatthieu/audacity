/*
* Audacity: A Digital Audio Editor
*/
import QtQuick

import MuseApi.Controls

//! NOTE Shipped (non-plugin) example cloud effect. Derives from CloudEffect (same module).
CloudEffect {
    id: root

    effectId: "dummy"

    params: ({ "intensity": 0.5, "option": false })

    Column {
        anchors.fill: parent
        spacing: 16

        StyledTextLabel {
            text: "Dummy cloud effect"
        }

        StyledTextLabel {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "A shipped (non-plugin) cloud effect, inheriting CloudEffect. The controls below build the opaque params."
        }

        CheckBox {
            text: "Some option"
            checked: root.params.option === true
            onClicked: root.params = Object.assign({}, root.params, { "option": !root.params.option })
        }

        StyledTextLabel {
            opacity: 0.6
            text: "params = " + JSON.stringify(root.params)
        }
    }
}
