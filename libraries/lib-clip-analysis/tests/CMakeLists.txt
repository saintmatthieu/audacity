add_compile_definitions(CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

add_unit_test(
   NAME
      lib-clip-analysis
   MOCK_PREFS
   WAV_FILE_IO
   SOURCES
      CircularClipTests.cpp
      GetBeatIndicesTests.cpp
      GetBpmFromOdfTests.cpp
      GetBpmFromOdfXcorrTests.cpp
      FloatVectorClip.cpp
      FloatVectorClip.h
      ResamplingClipTests.cpp
   LIBRARIES
      lib-clip-analysis
)
