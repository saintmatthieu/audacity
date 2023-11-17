#[[
todo
]]

set( LIBVAMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../lib-src/libvamp" )
set( LIBVAMP_INCLUDE_DIR "${LIBVAMP_DIR}/vamp-sdk" )
set( LIBVAMP_SOURCE_DIR "${LIBVAMP_DIR}/src/vamp-sdk" )

set( SOURCES
   ${LIBVAMP_INCLUDE_DIR}/FFT.h
   ${LIBVAMP_INCLUDE_DIR}/plugguard.h
   ${LIBVAMP_INCLUDE_DIR}/Plugin.h
   ${LIBVAMP_INCLUDE_DIR}/PluginAdapter.h
   ${LIBVAMP_INCLUDE_DIR}/PluginBase.h
   ${LIBVAMP_INCLUDE_DIR}/RealTime.h
   ${LIBVAMP_INCLUDE_DIR}/vamp-sdk.h

   ${LIBVAMP_SOURCE_DIR}/acsymbols.c
   ${LIBVAMP_SOURCE_DIR}/FFT.cpp
   ${LIBVAMP_SOURCE_DIR}/FFTimpl.cpp
   ${LIBVAMP_SOURCE_DIR}/PluginAdapter.cpp
   ${LIBVAMP_SOURCE_DIR}/RealTime.cpp
)

set( LIBRARIES
   libvamp
)

set ( TARGET_ROOT
   ${LIBVAMP_DIR}
)

audacity_library( lib-vamp-plugin "${SOURCES}" "${LIBRARIES}"
   "" "" ${TARGET_ROOT}
)
