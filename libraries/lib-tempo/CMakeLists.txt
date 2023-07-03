#[[
todo(mhodgkinson)
]]

set( SOURCES
   BeatsPerMinute.cpp
   BeatsPerMinute.h
)

set( LIBRARIES
   lib-preferences-interface
)
audacity_library( lib-tempo "${SOURCES}" "${LIBRARIES}"
   "" ""
)

