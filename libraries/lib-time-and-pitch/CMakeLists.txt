set( SOURCES
   StaffPad/pffft/pffft.c
   StaffPad/FourierTransform_pffft.cpp
   StaffPad/TimeAndPitch.cpp
   StaffPadTimeAndPitch.cpp
   TimeAndPitchInterface.h
)
set( LIBRARIES
)
audacity_library( lib-time-and-pitch "${SOURCES}" ""
   "" ""
)
