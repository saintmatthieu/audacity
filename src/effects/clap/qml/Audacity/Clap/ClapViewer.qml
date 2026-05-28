/*
 * Audacity: A Digital Audio Editor
 */
import QtQuick

import Muse.Ui
import Muse.UiComponents

import Audacity.Effects
import Audacity.Clap

Rectangle {
    id: root

    // in
    required property int instanceId
    property alias sidePadding: view.sidePadding
    property alias topPadding: view.topPadding
    property alias bottomPadding: view.bottomPadding
    property alias minimumWidth: view.minimumWidth

    // out
    property alias title: view.title
    property bool isPreviewing: viewModel.isPreviewing

    color: ui.theme.backgroundPrimaryColor

    implicitWidth: view.implicitWidth
    implicitHeight: view.implicitHeight

    readonly property var viewModel: ClapViewModelFactory.createModel(root, instanceId)

    Component.onCompleted: {
        viewModel.init()
        view.init()
    }

    function startPreview() {
        viewModel.startPreview()
    }

    function stopPreview() {
        viewModel.stopPreview()
    }

    ClapView {
        id: view
        instanceId: viewModel.instanceId
    }
}
