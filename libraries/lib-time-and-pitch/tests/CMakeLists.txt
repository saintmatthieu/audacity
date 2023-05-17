add_compile_definitions(CMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

add_unit_test(
   NAME
      lib-time-and-pitch
   SOURCES
      StaffPadTimeAndPitchTest.cpp
      WavFileIO.cpp
      WavFileIO.h
   LIBRARIES
      lib-utility
      lib-time-and-pitch-interface
      SndFile::sndfile
)
