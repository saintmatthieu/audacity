#[[
Library for Music Information Retrieval (MIR) algorithms.
]]

set( SOURCES
   FixedTempoEstimator.cpp
   FixedTempoEstimator.h
   GetBeats.cpp
   GetBeats.h
   GetBeatsUsingQueenMaryBarBeatTracking.cpp
   GetBeatsUsingQueenMaryBarBeatTracking.h
   MirAudioSource.h
   MirTypes.h
   MusicInformationRetrieval.cpp
   MusicInformationRetrieval.h
)

set( LIBRARIES
   libvampp
   libvamp
)

audacity_library( lib-music-information-retrieval "${SOURCES}" "${LIBRARIES}"
   "" ""
)
