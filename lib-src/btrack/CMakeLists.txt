cmake_minimum_required (VERSION 3.12)
project (BTrack VERSION 1.0.4)

set (CMAKE_CXX_STANDARD 11)

include_directories(C:/Users/saint/Downloads/libsamplerate-master/libsamplerate-master/build/install/include)

add_subdirectory (src)

    enable_testing()
    add_subdirectory (tests)

set (CMAKE_SUPPRESS_REGENERATION true)

