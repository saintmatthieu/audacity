#[[
Library for Music Information Retrieval (MIR) algorithms.
]]

set( SOURCES
   GetBeats.cpp
   GetBeats.h
   GetBeatsUsingBTrack.cpp
   GetBeatsUsingBTrack.h
   GetBeatsUsingQueenMaryBarBeatTracking.cpp
   GetBeatsUsingQueenMaryBarBeatTracking.h
   GetVampFeatures.cpp
   GetVampFeatures.h
   MirAudioSource.h
   MirTypes.h
   MusicInformationRetrieval.cpp
   MusicInformationRetrieval.h
)

set( LIBRARIES
   btrack
   libvampp
   libvamp
   lib-utility
   lib-vamp-plugins-interface
   lib-math
)

audacity_library( lib-music-information-retrieval "${SOURCES}" "${LIBRARIES}"
   "" ""
)
