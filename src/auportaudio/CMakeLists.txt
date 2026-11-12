#
# Audacity: A Digital Audio Editor
#

declare_module(auportaudio)

set(MODULE_SRC
    ${CMAKE_CURRENT_LIST_DIR}/auportaudiomodule.cpp
    ${CMAKE_CURRENT_LIST_DIR}/auportaudiomodule.h
    ${CMAKE_CURRENT_LIST_DIR}/iportaudiodriver.h

    ${CMAKE_CURRENT_LIST_DIR}/internal/portaudiodevice.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/portaudiodevice.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/portaudiodevices.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/portaudiodevices.h
)

set(MODULE_LINK
    audio
    au3wrap
    au3-audio-devices
)

setup_module()
