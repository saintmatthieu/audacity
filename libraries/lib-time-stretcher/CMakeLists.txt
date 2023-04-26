set( SOURCES
   TimeStretcher.cpp
   TimeStretcher.h
   TimeStretcherInterface.h
)
set( LIBRARIES
)
audacity_library( lib-time-stretcher "${SOURCES}" ""
   "" ""
)
