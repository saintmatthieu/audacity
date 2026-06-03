#
# Audacity: A Digital Audio Editor
#

# Bundled extensions are installed next to the other muse framework
# resources, where ExtensionsConfiguration::defaultPath() looks for them
# (globalConfiguration()->appDataPath() + "/extensions").
install(DIRECTORY .
    DESTINATION ${MUSE_APP_INSTALL_RESOURCES_LOCATION}extensions
    FILES_MATCHING
    PATTERN "*.json"
    PATTERN "*.qml"
    PATTERN "*.js"
)
