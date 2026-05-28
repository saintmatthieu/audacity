#[[
CLAP effect plugin discovery and processing logic, without user interface
]]

set( TARGET au3-clap )

set( SOURCES
   ClapEffectBase.cpp
   ClapEffectBase.h
   ClapEffectsProvider.cpp
   ClapEffectsProvider.h
   ClapEntry.cpp
   ClapEntry.h
   ClapInstance.cpp
   ClapInstance.h
   ClapParameterExtraction.cpp
   ClapParameterExtraction.h
   ClapSettings.h
   ClapUtils.cpp
   ClapUtils.h
   ClapWrapper.cpp
   ClapWrapper.h
   IClapHostListener.h
)
set( LIBRARIES
   au3-utility
   au3-effects-interface
   au3-files-interface
   PRIVATE
   wxBase
)
audacity_library( ${TARGET} "${SOURCES}" "${LIBRARIES}"
   "" ""
)

# CLAP SDK is header-only; expose the vendored submodule headers.
target_include_directories( ${TARGET} PRIVATE
   "${CMAKE_CURRENT_LIST_DIR}/../../../thirdparty/clap/include"
   "${CMAKE_CURRENT_LIST_DIR}/../../../thirdparty/clap-helpers/include"
)
