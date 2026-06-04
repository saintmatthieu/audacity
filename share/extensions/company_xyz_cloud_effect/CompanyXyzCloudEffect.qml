import QtQuick

import MuseApi.Controls
import Audacity.CloudEffects

//! NOTE Example cloud-effect extension.
//! Subclasses CloudEffect (from MuseApi.Controls): it only provides the
//! UI, the effect id, and the opaque `params`. The Cancel / Apply bar — and closing
//! the window on either — come from the base.
CloudEffect {
    id: root

    effectId: "xyz.example-cloud-effect"

    //! NOTE Opaque params. The host never interprets these; the schema is shared
    //! between this UI (producer) and the 3rd-party service (consumer).
    params: ({
            "denoise": true,
            "normalize": false
        })

    Column {
        anchors.fill: parent
        spacing: 16

        StyledTextLabel {
            text: "Super cloud effect by XYZ"
        }

        StyledTextLabel {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "The controls below build the opaque `params` object that gets sent, unchanged, to the cloud service."
        }

        CheckBox {
            text: "Denoise"
            checked: root.params.denoise
            onClicked: root.params = Object.assign({}, root.params, {
                "denoise": !root.params.denoise
            })
        }

        CheckBox {
            text: "Normalize"
            checked: root.params.normalize
            onClicked: root.params = Object.assign({}, root.params, {
                "normalize": !root.params.normalize
            })
        }

        StyledTextLabel {
            opacity: 0.6
            text: "params = " + JSON.stringify(root.params)
        }
    }
}
