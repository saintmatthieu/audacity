/*
* Audacity: A Digital Audio Editor
*/
import QtQuick
import QtQuick.Window

import MuseApi.Controls
import MuseApi.Theme

/** APIDOC
 * Base class for a cloud-effect extension form.
 *
 * Subclass it, fill it with your UI, set `effectId` and optionally `params`.
 * The Cancel / Apply bar is provided; pressing Apply invokes `apply()`.
 *
 * @class CloudEffect
 * @hideconstructor
 * @property {string} effectId - Identifies the effect for the audio.com provider registry (required).
 * @property {object} params - Opaque, host-pass-through parameters consumed by the 3rd-party service.
 *                             The host never interprets these; the schema is shared between this UI
 *                             and the vendor's service.
 * @example
 * import Audacity.CloudEffects
 *
 * CloudEffect {
 *     effectId: "my-vendor.denoise"
 *
 *     // ... your UI here (laid out above the Cancel / Apply bar) ...
 * }
*/
Rectangle {
    id: root

    color: Theme.backgroundPrimaryColor

    implicitWidth: 480
    implicitHeight: 360

    //! NOTE Public API — see the CloudEffect class diagram.
    required property string effectId
    property var params: ({})

    //! NOTE Subclass UI goes here (above the Cancel / Apply bar).
    default property alias content: contentArea.data

    //! NOTE Emitted when the user presses Cancel.
    signal canceled

    //! NOTE Composed model: shows the toast and (later) collects project/selection
    //! and calls IAu3AudioComService.
    property CloudEffectModel _model: CloudEffectModel {}

    //! NOTE Invoked when the user presses Apply. Forwards to the model (which shows the
    //! toast and, later, runs the cloud job), then closes the window.
    function apply() {
        root._model.apply(root.effectId, root.params);

        //! NOTE Close the hosting window (host-agnostic).
        if (Window.window) {
            Window.window.close()
        }
    }

    //! NOTE Assigned explicitly so the base chrome is NOT captured by the
    //! `content` default-property alias above.
    children: [
        Item {
            id: contentArea

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: separator.top
            anchors.margins: 16
        },
        SeparatorLine {
            id: separator

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: buttonsRow.top
            anchors.bottomMargin: 12
        },
        Row {
            id: buttonsRow

            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 16

            spacing: 8

            FlatButton {
                text: "Cancel"
                onClicked: {
                    root.canceled();

                    //! NOTE Close the hosting window (host-agnostic).
                    if (Window.window) {
                        Window.window.close()
                    }
                }
            }

            FlatButton {
                text: "Apply"
                accentButton: true
                onClicked: root.apply()
            }
        }
    ]
}
