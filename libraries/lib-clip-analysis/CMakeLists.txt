#[[
Provides audio clip analysis functionality.
]]

set( SOURCES
   AudioSegmentSampleView.cpp
   AudioSegmentSampleView.h
   CircularSampleBuffer.h
   CircularClip.cpp
   CircularClip.h
   ClipAnalysis.cpp
   ClipAnalysis.h
   ClipAnalysisUtils.cpp
   ClipAnalysisUtils.h
   ClipInterface.cpp
   ClipInterface.h
   ClipInterfacePartialImpl.h
   GetBpmFromOdf.cpp
   GetBpmFromOdf.h
   GetBpmFromOdfXcorr.cpp
   GetBpmFromOdfXcorr.h
   OnsetDetector.cpp
   OnsetDetector.h
   ResamplingClip.cpp
   ResamplingClip.h
   SpectrumTransformer.cpp
   SpectrumTransformer.h
   TimeDiv.cpp
   TimeDiv.h

   NamedType/named_type.hpp

   DSPFilters/Bessel.cpp
   DSPFilters/Biquad.cpp
   DSPFilters/Butterworth.cpp
   DSPFilters/Cascade.cpp
   DSPFilters/ChebyshevI.cpp
   DSPFilters/ChebyshevII.cpp
   DSPFilters/Custom.cpp
   DSPFilters/Design.cpp
   DSPFilters/Documentation.cpp
   DSPFilters/Elliptic.cpp
   DSPFilters/Filter.cpp
   DSPFilters/Legendre.cpp
   DSPFilters/Param.cpp
   DSPFilters/PoleFilter.cpp
   DSPFilters/RBJ.cpp
   DSPFilters/RootFinder.cpp
   DSPFilters/State.cpp
)
set( LIBRARIES
   lib-math
)
audacity_library( lib-clip-analysis "${SOURCES}" "${LIBRARIES}"
   "" ""
)
