#
# Audacity: A Digital Audio Editor
#

declare_module(au3audio)

set(MODULE_QML_IMPORT ${CMAKE_CURRENT_LIST_DIR}/qml)

set(MODULE_SRC
    #public
    ${CMAKE_CURRENT_LIST_DIR}/au3audiomodule.cpp
    ${CMAKE_CURRENT_LIST_DIR}/au3audiomodule.h
    ${CMAKE_CURRENT_LIST_DIR}/isystemaudiodeviceslistener.h

    # AU3
    ${CMAKE_CURRENT_LIST_DIR}/internal/au3audioengine.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au3audioengine.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/audiodrivercontroller.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/audiodrivercontroller.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/iaudiodevices.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/portaudiodevices.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/portaudiodevices.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au3audioiolistener.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au3audiometer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au3audiometer.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/defaultplaybackpolicy.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/defaultplaybackpolicy.h
    )

if (OS_IS_MAC)
    set(MODULE_SRC ${MODULE_SRC}
        ${CMAKE_CURRENT_LIST_DIR}/internal/platform/macos/macossystemaudiodeviceslistener.cpp
        ${CMAKE_CURRENT_LIST_DIR}/internal/platform/macos/macossystemaudiodeviceslistener.h
    )
    # the CoreAudio system headers clash with the `using namespace muse`
    # of other unity-combined sources
    set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/internal/platform/macos/macossystemaudiodeviceslistener.cpp
        PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
    )
elseif (OS_IS_WIN)
    set(MODULE_SRC ${MODULE_SRC}
        ${CMAKE_CURRENT_LIST_DIR}/internal/platform/windows/windowssystemaudiodeviceslistener.cpp
        ${CMAKE_CURRENT_LIST_DIR}/internal/platform/windows/windowssystemaudiodeviceslistener.h
    )
    # the Windows system headers clash with the `using namespace muse`
    # of other unity-combined sources
    set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/internal/platform/windows/windowssystemaudiodeviceslistener.cpp
        PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON
    )
elseif (OS_IS_LIN)
    find_path(LIBUDEV_INCLUDE_DIR libudev.h)
    find_library(LIBUDEV_LIBRARY udev)
    if (LIBUDEV_INCLUDE_DIR AND LIBUDEV_LIBRARY)
        set(MODULE_SRC ${MODULE_SRC}
            ${CMAKE_CURRENT_LIST_DIR}/internal/platform/linux/linuxsystemaudiodeviceslistener.cpp
            ${CMAKE_CURRENT_LIST_DIR}/internal/platform/linux/linuxsystemaudiodeviceslistener.h
        )
    else()
        message(STATUS "libudev not found - system audio device change detection disabled")
        set(MODULE_SRC ${MODULE_SRC}
            ${CMAKE_CURRENT_LIST_DIR}/internal/platform/stub/stubsystemaudiodeviceslistener.h
        )
    endif()
else()
    set(MODULE_SRC ${MODULE_SRC}
        ${CMAKE_CURRENT_LIST_DIR}/internal/platform/stub/stubsystemaudiodeviceslistener.h
    )
endif()

set(MODULE_LINK
        au3-audio-io
        au3wrap
        au3-audio-devices
        au3-math
        au3-project-rate
        au3-time-frequency-selection
        au3-track
        au3-utility
)

if (OS_IS_MAC)
    set(MODULE_LINK ${MODULE_LINK} "-framework CoreAudio")
elseif (OS_IS_WIN)
    set(MODULE_LINK ${MODULE_LINK} ole32)
elseif (OS_IS_LIN AND LIBUDEV_INCLUDE_DIR AND LIBUDEV_LIBRARY)
    set(MODULE_INCLUDE ${MODULE_INCLUDE} ${LIBUDEV_INCLUDE_DIR})
    set(MODULE_LINK ${MODULE_LINK} ${LIBUDEV_LIBRARY})
    set(MODULE_DEF ${MODULE_DEF} AU_AU3AUDIO_HAS_UDEV)
endif()

# The audio engine of graphs of nodes, over any audio library
set(AU4_SRC
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/au4audioengine.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/au4audioengine.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/blockcontext.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/blockcontext.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/captureinput.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/captureinput.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/effectscratch.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/effectscratch.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/effectslatency.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/effectslatency.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/effectsnode.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/effectsnode.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/endoftracknode.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/endoftracknode.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/fastiocallback.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/fastiocallback.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/gainpannode.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/gainpannode.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/idevice.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/ieffectscontext.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/iendoftrackcontext.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/imutesolocontext.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/iocallback.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/iocallback.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/metersendnode.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/metersendnode.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/metersink.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/metersink.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/metertapnode.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/metertapnode.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/monitoredinput.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/monitoredinput.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/monitoringcallback.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/monitoringcallback.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/mutesolonode.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/mutesolonode.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/playedrange.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/playheadmapper.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/playheadmapper.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/recordingcapture.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/recordingcapture.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/safeiocallback.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/safeiocallback.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/sequentialmixnode.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/sequentialmixnode.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/stereo.h
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/track.cpp
    ${CMAKE_CURRENT_LIST_DIR}/internal/au4/track.h
)
set(MODULE_SRC ${MODULE_SRC} ${AU4_SRC})
# Its `using namespace au::au4` would make au4::Track and au3's Track ambiguous in other unity-combined sources
set_source_files_properties(${AU4_SRC} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
set(MODULE_LINK ${MODULE_LINK}
    au3-clip-processing
    au3-mixer
    au3-project
    au3-realtime-effects
    au3-wave-track
)

if (AU_USE_JUCE)
    set(JUCE_SRC
        ${CMAKE_CURRENT_LIST_DIR}/internal/juce/juceaudiodevices.cpp
        ${CMAKE_CURRENT_LIST_DIR}/internal/juce/juceaudiodevices.h
        ${CMAKE_CURRENT_LIST_DIR}/internal/juce/jucedevice.cpp
        ${CMAKE_CURRENT_LIST_DIR}/internal/juce/jucedevice.h
        ${CMAKE_CURRENT_LIST_DIR}/internal/juce/juceincludes.h
    )
    set(MODULE_SRC ${MODULE_SRC} ${JUCE_SRC})
    # JUCE headers clash with the `using namespace muse` of other unity-combined sources
    set_source_files_properties(${JUCE_SRC} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    set(MODULE_LINK ${MODULE_LINK} juce)
    set(MODULE_DEF ${MODULE_DEF} AU_USE_JUCE)
endif()

if (AU_USE_TRACKTION)
    set(TRACKTION_SRC
        ${CMAKE_CURRENT_LIST_DIR}/internal/tracktion/audacitytotracktionnode.cpp
        ${CMAKE_CURRENT_LIST_DIR}/internal/tracktion/audacitytotracktionnode.h
        ${CMAKE_CURRENT_LIST_DIR}/internal/tracktion/tracktiongraphincludes.h
        ${CMAKE_CURRENT_LIST_DIR}/internal/tracktion/tracktiontoaudacitynode.cpp
        ${CMAKE_CURRENT_LIST_DIR}/internal/tracktion/tracktiontoaudacitynode.h
    )
    set(MODULE_SRC ${MODULE_SRC} ${TRACKTION_SRC})
    # tracktion_graph's headers include JUCE's
    set_source_files_properties(${TRACKTION_SRC} PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)
    set(MODULE_LINK ${MODULE_LINK} tracktion_graph)
    set(MODULE_DEF ${MODULE_DEF} AU_USE_TRACKTION)
endif()

setup_module()

if (AU_BUILD_AU3AUDIO_TESTS)
    add_subdirectory(tests)
endif()
