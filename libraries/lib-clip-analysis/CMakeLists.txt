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

   Bessel.cpp
   Biquad.cpp
   Butterworth.cpp
   Cascade.cpp
   ChebyshevI.cpp
   ChebyshevII.cpp
   Custom.cpp
   Design.cpp
   Documentation.cpp
   Elliptic.cpp
   Filter.cpp
   Legendre.cpp
   Param.cpp
   PoleFilter.cpp
   RBJ.cpp
   RootFinder.cpp
   State.cpp
)
set( LIBRARIES
   lib-math
)
audacity_library( lib-clip-analysis "${SOURCES}" "${LIBRARIES}"
   "" ""
)
