#[[
Provides audio clip analysis functionality.
]]

set( SOURCES
   AudioSegmentSampleView.cpp
   AudioSegmentSampleView.h
   ClipAnalysis.cpp
   ClipAnalysis.h
   ClipInterface.cpp
   ClipInterface.h
   ClipTimingEstimator.cpp
   ClipTimingEstimator.h
   SpectrumTransformer.cpp
   SpectrumTransformer.h
)
set( LIBRARIES
   lib-math
)
audacity_library( lib-clip-analysis "${SOURCES}" "${LIBRARIES}"
   "" ""
)
