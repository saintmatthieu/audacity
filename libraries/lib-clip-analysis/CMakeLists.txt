#[[
Provides audio clip analysis functionality.
]]

set( SOURCES
   AudioSegmentSampleView.cpp
   AudioSegmentSampleView.h
   CircularSampleBuffer.h
   ClipAnalysis.cpp
   ClipAnalysis.h
   ClipAnalysisUtils.cpp
   ClipAnalysisUtils.h
   ClipInterface.cpp
   ClipInterface.h
   GetBpmFromOdfXcorr.cpp
   GetBpmFromOdfXcorr.h
   OnsetDetector.cpp
   OnsetDetector.h
   SpectrumTransformer.cpp
   SpectrumTransformer.h
   TimeDiv.cpp
   TimeDiv.h
)
set( LIBRARIES
   lib-math
)
audacity_library( lib-clip-analysis "${SOURCES}" "${LIBRARIES}"
   "" ""
)
