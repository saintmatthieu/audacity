#[[
Unit tests for au3-clip-processing-sequence
]]

add_compile_definitions(CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

add_unit_test(
   NAME
      au3-clip-processing-sequence
   SOURCES
      AudioContainerHelper.h
      AudioSegmentSampleViewTest.cpp
      ClipSegmentTest.cpp
      ClipTimeAndPitchSourceTest.cpp
      FloatVectorClip.cpp
      FloatVectorClip.h
      MockAudioSegmentFactory.h
      MockSampleBlock.cpp
      MockSampleBlock.h
      MockSampleBlockFactory.cpp
      MockSampleBlockFactory.h
      MockPlayableSequence.h
      SilenceSegmentTest.cpp
      ClipProcessingSequenceTest.cpp
      ClipProcessingSequenceIntegrationTest.cpp
      TestWaveClipMaker.cpp
      TestWaveClipMaker.h
      TestWaveTrackMaker.cpp
      TestWaveTrackMaker.h
   MOCK_PREFS
   MOCK_AUDIO
   WAV_FILE_IO
   LIBRARIES
      au3-clip-processing-sequence
      au3-wave-track
)
