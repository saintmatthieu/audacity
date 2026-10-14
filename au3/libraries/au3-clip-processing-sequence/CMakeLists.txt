#[[
A library providing a time-stretching-capable implementation of
`PlayableSequence`.

Please refer to this directory's readme for a presentation of the constraints
inherent to such an implementation.
]]

set( TARGET au3-clip-processing-sequence )

set( SOURCES
   AudioSegment.cpp
   AudioSegment.h
   AudioSegmentFactory.cpp
   AudioSegmentFactory.h
   AudioSegmentFactoryInterface.cpp
   AudioSegmentFactoryInterface.h
   AudioSegmentSampleView.cpp
   AudioSegmentSampleView.h
   ClipFades.cpp
   ClipFades.h
   ClipInterface.cpp
   ClipInterface.h
   ClipSegment.cpp
   ClipSegment.h
   PlaybackDirection.h
   ClipProcessingSequence.cpp
   ClipProcessingSequence.h
   ClipTimeAndPitchSource.cpp
   ClipTimeAndPitchSource.h
   TempoChange.cpp
   TempoChange.h
)
set( LIBRARIES
PUBLIC
   au3-math
   au3-mixer
   au3-registries
   au3-time-and-pitch
   au3-utility
PRIVATE
   au3-channel
   au3-strings
   au3-xml
   wxBase
)
audacity_library( ${TARGET} "${SOURCES}" "${LIBRARIES}"
   "" ""
)
