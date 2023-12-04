add_library( ${TARGET} STATIC )

def_vars()

set( SOURCES
   PRIVATE
      ${TARGET_ROOT}/dsp/chromagram/Chromagram.cpp
      ${TARGET_ROOT}/dsp/chromagram/Chromagram.h
      ${TARGET_ROOT}/dsp/mfcc/MFCC.cpp
      ${TARGET_ROOT}/dsp/mfcc/MFCC.h
      ${TARGET_ROOT}/dsp/onsets/DetectionFunction.cpp
      ${TARGET_ROOT}/dsp/onsets/DetectionFunction.h
      ${TARGET_ROOT}/dsp/phasevocoder/PhaseVocoder.cpp
      ${TARGET_ROOT}/dsp/phasevocoder/PhaseVocoder.h
      ${TARGET_ROOT}/dsp/rateconversion/Decimator.cpp
      ${TARGET_ROOT}/dsp/rateconversion/Decimator.h
      ${TARGET_ROOT}/dsp/tempotracking/DownBeat.cpp
      ${TARGET_ROOT}/dsp/tempotracking/DownBeat.h
      ${TARGET_ROOT}/dsp/tempotracking/TempoTrackV2.cpp
      ${TARGET_ROOT}/dsp/tempotracking/TempoTrackV2.h
      ${TARGET_ROOT}/dsp/transforms/FFT.cpp
      ${TARGET_ROOT}/dsp/transforms/FFT.h
      ${TARGET_ROOT}/maths/MathUtilities.cpp
      ${TARGET_ROOT}/maths/MathUtilities.h

      ${TARGET_ROOT}/ext/kissfft/kiss_fft.c
      ${TARGET_ROOT}/ext/kissfft/tools/kiss_fftr.c
)

organize_source( "${TARGET_ROOT}" "" "${SOURCES}" )
target_sources( ${TARGET} PRIVATE ${SOURCES} )
target_compile_definitions( ${TARGET} PRIVATE _USE_MATH_DEFINES )
target_include_directories( ${TARGET}
PUBLIC
   ${TARGET_ROOT}
   ${TARGET_ROOT}/include
PRIVATE
   ${TARGET_ROOT}/ext/kissfft
   ${TARGET_ROOT}/ext/kissfft/tools
)

set_target_properties(${TARGET} PROPERTIES POSITION_INDEPENDENT_CODE On)
