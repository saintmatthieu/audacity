# SPDX-License-Identifier: MIT
#
# Standalone build for a minimal CLAP plugin that embeds a WebKit2GTK webview
# as its native UI. Used to validate CLAP X11 GUI embedding (Phase 2) with a
# webview-based plugin.

cmake_minimum_required(VERSION 3.16)
project(clap-webview-demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(WEBKIT2 REQUIRED IMPORTED_TARGET webkit2gtk-4.1)

# A .clap on Linux is just a shared library named *.clap exporting the
# `clap_entry` symbol.
add_library(clap-webview-demo MODULE clap_webview_demo.cpp)

set_target_properties(clap-webview-demo PROPERTIES
    PREFIX ""
    OUTPUT_NAME "clap-webview-demo"
    SUFFIX ".clap"
)

target_compile_options(clap-webview-demo PRIVATE -Wall -Wextra -Wno-unused-parameter)

target_include_directories(clap-webview-demo PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../thirdparty/clap/include
)
target_link_libraries(clap-webview-demo PRIVATE PkgConfig::WEBKIT2)

# Convenience: `make install` deploys the plugin to the user-level CLAP path.
include(GNUInstallDirs)
install(TARGETS clap-webview-demo
    LIBRARY DESTINATION "$ENV{HOME}/.clap"
)
