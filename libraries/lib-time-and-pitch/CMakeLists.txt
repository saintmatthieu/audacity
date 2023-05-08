set( SOURCES
   StaffPad/pffft/pffft.c
   StaffPad/FourierTransform_pffft.cpp
   StaffPad/TimeAndPitch.cpp
   AudioContainer.cpp
   StaffPadTimeAndPitch.cpp
   TimeAndPitchInterface.cpp
)
set( LIBRARIES
)
audacity_library( lib-time-and-pitch "${SOURCES}" ""
   "" ""
)
