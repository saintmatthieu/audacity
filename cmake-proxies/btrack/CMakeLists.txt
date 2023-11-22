add_library( ${TARGET} STATIC )

def_vars()

set( SOURCES
   PRIVATE
      ${TARGET_ROOT}/src/BTrack.cpp
      ${TARGET_ROOT}/src/BTrack.h
      ${TARGET_ROOT}/src/CircularBuffer.h
      ${TARGET_ROOT}/src/OnsetDetectionFunction.cpp
      ${TARGET_ROOT}/src/OnsetDetectionFunction.h
      ${TARGET_ROOT}/libs/kiss_fft130/kiss_fft.c
)

organize_source( "${TARGET_ROOT}" "" "${SOURCES}" )
target_sources( ${TARGET} PRIVATE ${SOURCES} )
target_compile_definitions( ${TARGET} PUBLIC USE_KISS_FFT )
target_include_directories( ${TARGET} PUBLIC
   ${TARGET_ROOT}/libs/kiss_fft130
   ${TARGET_ROOT}/src
   )
target_link_libraries( ${TARGET} PUBLIC libsoxr )

set_target_properties(${TARGET} PROPERTIES POSITION_INDEPENDENT_CODE On)
