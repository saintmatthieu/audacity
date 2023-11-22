add_library( audacity-vamp-plugins SHARED
   AudacityVampPlugins.cpp
   MyVampPlugin.cpp
   MyVampPlugin.h
)

target_link_options( audacity-vamp-plugins PUBLIC /EXPORT:vampGetPluginDescriptor )
target_link_libraries( audacity-vamp-plugins PRIVATE btrack libvampp )
