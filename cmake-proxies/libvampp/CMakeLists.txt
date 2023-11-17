
add_library( libvampp STATIC )

def_vars()

set ( TARGET_ROOT ${CMAKE_SOURCE_DIR}/lib-src/libvamp )

set( SOURCES
   PUBLIC
   ${TARGET_ROOT}/src/vamp-sdk/FFT.cpp
   ${TARGET_ROOT}/src/vamp-sdk/PluginAdapter.cpp
   ${TARGET_ROOT}/src/vamp-sdk/RealTime.cpp
)

set( INCLUDES
      ${TARGET_ROOT}
)

set( DEFINES
      _USE_MATH_DEFINES
)

organize_source( "${TARGET_ROOT}" "" "${SOURCES}" )
target_sources( libvampp PUBLIC ${SOURCES} )
target_compile_definitions( libvampp PUBLIC ${DEFINES} )
target_include_directories( libvampp PUBLIC ${INCLUDES} )

set_target_properties(libvampp PROPERTIES POSITION_INDEPENDENT_CODE On)
