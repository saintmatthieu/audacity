#[[
TODO
]]#

set( SOURCES
   AudacityVampPlugins.cpp
   AudacityVampUtils.cpp
   AudacityVampUtils.h
   BeatSynchronousChromagram.cpp
   BeatSynchronousChromagram.h
)

set( LIBRARIES
   btrack
   libvampp
)

audacity_library( lib-vamp-plugins "${SOURCES}" "${LIBRARIES}"
   "" ""
)

target_link_options( lib-vamp-plugins PUBLIC /EXPORT:vampGetPluginDescriptor )
