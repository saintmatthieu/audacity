add_library( ${TARGET} STATIC )

def_vars()

set( SOURCES
   PRIVATE
   ${TARGET_ROOT}/src/source/ExpectationMaximization.cpp
   ${TARGET_ROOT}/src/header/GaussianMixtureModel/ExpectationMaximization.h
)

organize_source( "${TARGET_ROOT}" "" "${SOURCES}" )
target_sources( ${TARGET} PRIVATE ${SOURCES} )
target_include_directories( ${TARGET}
PUBLIC
   ${TARGET_ROOT}/src/header
PRIVATE
   ${TARGET_ROOT}/src
)

set_target_properties(${TARGET} PROPERTIES POSITION_INDEPENDENT_CODE On)
