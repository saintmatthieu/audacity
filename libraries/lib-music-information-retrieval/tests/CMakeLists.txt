# Get commit hash
execute_process(
   COMMAND git rev-parse HEAD
   WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
   OUTPUT_VARIABLE GIT_COMMIT_HASH
   OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_compile_definitions(
   CMAKE_CURRENT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
   GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
)

add_unit_test(
   NAME
      lib-music-information-retrieval
   WAV_FILE_IO
   SOURCES
      MusicInformationRetrievalTests.cpp
      WavMirAudioSource.cpp
      WavMirAudioSource.h
   LIBRARIES
      lib-music-information-retrieval
)
