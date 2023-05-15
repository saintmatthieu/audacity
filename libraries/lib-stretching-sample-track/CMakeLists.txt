set( SOURCES
   StretchingPlaybackTrackFactory.cpp
   StretchingPlaybackTrackFactory.h
   StretchingSampleTrack.cpp
   StretchingSampleTrack.h
)
set( LIBRARIES
   lib-audio-io-interface
   lib-time-and-pitch-interface
   lib-sample-track-interface
   lib-wave-track-interface
)
audacity_library( lib-stretching-sample-track "${SOURCES}" "${LIBRARIES}"
   "" ""
)
