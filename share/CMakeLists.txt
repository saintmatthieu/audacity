#
# Audacity: A Digital Audio Editor
#

add_subdirectory(locale)
add_subdirectory(testflowscripts)
add_subdirectory(icons)
add_subdirectory(workspaces)

if (MUSE_MODULE_EXTENSIONS)
    add_subdirectory(extensions)
endif()

IF (AU_MODULE_EFFECTS_NYQUIST)
    add_subdirectory(nyquist-runtime)
    add_subdirectory(nyquist-plug-ins)
ENDIF()