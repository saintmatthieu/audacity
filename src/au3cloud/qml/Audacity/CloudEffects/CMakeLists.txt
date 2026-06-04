#
# Audacity: A Digital Audio Editor
#

muse_create_qml_module(au3cloud_qml FOR au3cloud)

qt_add_qml_module(au3cloud_qml
    URI Audacity.CloudEffects
    VERSION 1.0
    SOURCES
        cloudeffectmodel.cpp
        cloudeffectmodel.h
    QML_FILES
        CloudEffect.qml
        DummyCloudEffect.qml
        CloudEffectDialog.qml
    IMPORTS
        QtQuick
        TARGET muse_uicomponents_qmlapi
        TARGET muse_ui_qml
        TARGET muse_uicomponents_qml
)
