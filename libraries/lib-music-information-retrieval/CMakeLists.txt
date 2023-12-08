#[[
Library for Music Information Retrieval (MIR) algorithms.
]]

set( SOURCES
   # BeatFinder.cpp
   # BeatFinder.h
   # GetBeats.cpp
   # GetBeats.h
   # GetBeatsUsingBTrack.cpp
   # GetBeatsUsingBTrack.h
   # GetBeatsUsingQueenMaryBarBeatTracking.cpp
   # GetBeatsUsingQueenMaryBarBeatTracking.h
   # GetVampFeatures.cpp
   # GetVampFeatures.h
   # GetVampFeaturesFromPlugin.h
   # QueenMaryBeatFinder.cpp
   # QueenMaryBeatFinder.h
   MirAudioSource.h
   MirTypes.h
   MusicInformationRetrieval.cpp
   MusicInformationRetrieval.h
   StftFrameProvider.cpp
   StftFrameProvider.h
)

set( LIBRARIES
   # btrack
   # libvampp
   # libvamp
   lib-utility
   # lib-vamp-plugins-interface
   lib-math
   # qm-vamp-plugins
   # Gaussian-Mixture-Model
   # Gaussian-Utils
)

audacity_library( lib-music-information-retrieval "${SOURCES}" "${LIBRARIES}"
   "" ""
)
