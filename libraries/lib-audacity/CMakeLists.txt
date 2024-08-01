#[[
A UI-agnostic, high-level library implementing the behavior of Audacity.
]]

set( SOURCES
   CompressorInstance.cpp
   CompressorInstance.h
   EffectManager.cpp
   EffectManager.h
   GetMessageBoxCb.h
   IAudacityCommand.h
   LibAudacityApi.cpp
   LibAudacityApi.h
   LibAudacityTypes.h
   ShuttleGetDefinition.cpp
   ShuttleGetDefinition.h
)
set( LIBRARIES
   lib-audio-io-interface
   lib-dynamic-range-processor-interface
   lib-effects-interface
   lib-menus-interface
   lib-project-file-io-interface
   lib-viewport-interface
)
audacity_library( lib-audacity "${SOURCES}" "${LIBRARIES}"
   "" ""
)
