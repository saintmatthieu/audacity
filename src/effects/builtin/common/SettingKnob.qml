import QtQuick 2.15

Item {
    id: root

    required property var model
    required property string title
    property string unit: ""
    property bool isBig: false

    Component.onCompleted: {
        model.init()
    }

    Loader {
        id: knobLoader

        onLoaded: {
            root.width = knobLoader.item.width
            root.height = knobLoader.item.height
        }
        sourceComponent: isBig ? bigKnob : smallKnob
    }

    Component {
        id: bigKnob

        BigParameterKnob {
            parameter: {
                "title": root.title,
                "unit": root.unit,
                "min": root.model.min,
                "max": root.model.max,
                "value": root.model.value,
                "step": root.model.step
            }

            onNewValueRequested: function(_, newValue) {
                root.model.value = newValue
            }

            onCommitRequested: {
                root.model.commitSettings()
            }
        }
    }

    Component {
        id: smallKnob

        ParameterKnob {
            parameter: {
                "title": root.title,
                "unit": root.unit,
                "min": root.model.min,
                "max": root.model.max,
                "value": root.model.value,
                "step": root.model.step
            }

            onNewValueRequested: function(_, newValue) {
                root.model.value = newValue
            }

            onCommitRequested: {
                root.model.commitSettings()
            }
        }
    }
}
