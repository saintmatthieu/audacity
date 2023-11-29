add_library( ${TARGET} STATIC )

def_vars()

set( SOURCES
   PRIVATE
      ${TARGET_ROOT}/plugins/BarBeatTrack.cpp
      ${TARGET_ROOT}/plugins/BarBeatTrack.h
)

organize_source( "${TARGET_ROOT}" "" "${SOURCES}" )
target_sources( ${TARGET} PRIVATE ${SOURCES} )
target_include_directories( ${TARGET} PUBLIC
   ${TARGET_ROOT}/lib
   ${TARGET_ROOT}/plugins
   )

set_target_properties(${TARGET} PROPERTIES POSITION_INDEPENDENT_CODE On)
target_link_libraries( ${TARGET} PUBLIC libvampp qm-dsp)
