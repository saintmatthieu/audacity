#[[
Library for Music Information Retrieval (MIR) algorithms.
]]

set( SOURCES
   FixedTempoEstimator.cpp
   FixedTempoEstimator.h
   MirAudioSource.h
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
