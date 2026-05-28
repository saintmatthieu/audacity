#
# Audacity: A Digital Audio Editor
#
declare_module(effects_clap)

set(MODULE_QRC clap.qrc)
set(MODULE_QML_IMPORT ${CMAKE_CURRENT_LIST_DIR}/qml)

set(MODULE_SRC
    ${CMAKE_CURRENT_LIST_DIR}/clapeffectsmodule.cpp
    ${CMAKE_CURRENT_LIST_DIR}/clapeffectsmodule.h

    ${CMAKE_CURRENT_LIST_DIR}/internal/clapeffectloader.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/clappluginsscanner.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/clappluginsmetareader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/clappluginsmetareader.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/clapparameterextractorservice.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/clapparameterextractorservice.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/clapviewlauncher.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/clapviewlauncher.h

    ${CMAKE_CURRENT_LIST_DIR}/view/clapview.cpp
    ${CMAKE_CURRENT_LIST_DIR}/view/clapview.h
    ${CMAKE_CURRENT_LIST_DIR}/view/clapviewmodel.cpp
    ${CMAKE_CURRENT_LIST_DIR}/view/clapviewmodel.h
)

# AU3
include(${CMAKE_CURRENT_LIST_DIR}/../../au3wrap/au3wrapDefs.cmake)

set(MODULE_INCLUDE ${AU3_INCLUDE})
set(MODULE_DEF ${AU3_DEF})

set(MODULE_LINK
        au3wrap
)

set(MODULE_USE_UNITY OFF)
setup_module()
