#[[
A library providing a time-stretching-capable implementation of `PlayableSequence`.

Please refer to this directory's readme for a presentation of the constraints inherent to such an implementation.
]]
set( SOURCES
   AudioSegment.cpp
   AudioSegment.h
   AudioSegmentFactory.cpp
   AudioSegmentFactory.h
   AudioSegmentFactoryInterface.cpp
   AudioSegmentFactoryInterface.h
   ClipSegment.cpp
   ClipSegment.h
   PlaybackDirection.h
   SilenceSegment.cpp
   SilenceSegment.h
   StretchingSequence.cpp
   StretchingSequence.h
   ClipTimeAndPitchSource.cpp
   ClipTimeAndPitchSource.h
)
set( LIBRARIES
   lib-clip-analysis
   lib-mixer
   lib-time-and-pitch
)
audacity_library( lib-stretching-sequence "${SOURCES}" "${LIBRARIES}"
   "" ""
)
