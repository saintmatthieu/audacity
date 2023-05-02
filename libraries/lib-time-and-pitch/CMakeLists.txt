set( SOURCES
   StaffPad/pffft/pffft.c
   StaffPad/FourierTransform_pffft.cpp
   StaffPad/TimeAndPitch.cpp
   StaffPadTimeAndPitch.cpp
   TimeAndPitchInterface.cpp
   TimeAndPitchInterface.h
   WaveTrackTimeStretcher.cpp
   WaveTrackTimeStretcher.h
)
set( LIBRARIES
   lib-wave-track-interface
)
audacity_library( lib-time-and-pitch "${SOURCES}" "${LIBRARIES}"
   "" ""
)
