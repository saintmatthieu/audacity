
add_library( ${TARGET} STATIC )

def_vars()

set( SOURCES
   PRIVATE
      ${TARGET_ROOT}/src/mvamp/MarsyasIBT.cpp
)

set( INCLUDES
   PUBLIC
      ${TARGET_ROOT}/src
)

set( DEFINES
   PRIVATE
      _USE_MATH_DEFINES
)

organize_source( "${TARGET_ROOT}" "" "${SOURCES}" )
target_sources( ${TARGET} PRIVATE ${SOURCES} )
target_compile_definitions( ${TARGET} PRIVATE ${DEFINES} )
target_include_directories( ${TARGET} PRIVATE ${INCLUDES} )

set_target_properties(${TARGET} PROPERTIES POSITION_INDEPENDENT_CODE On)
