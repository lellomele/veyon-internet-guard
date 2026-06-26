#
# InternetGuard - Veyon teacher-side "Block / Allow Internet" plugin
#
# Multi-version build: the same sources compile against every Veyon release
# from 4.7.5 up to 4.10.x. The only build-time choice is the Qt major version
# the *target* Veyon was compiled with (Qt5 by default, Qt6 with -DWITH_QT6=ON).
#
# Output names:
#   WITH_QT6=OFF  ->  internet-guard-qt5.dll
#   WITH_QT6=ON   ->  internet-guard-qt6.dll
#
# Configurable cache variables:
#   WITH_QT6             OFF      Build against Qt6 instead of Qt5
#   VEYON_TARGET_VERSION 4.7.5    Target Veyon version (drives compat macros / docs)
#   VEYON_SOURCE_DIR     ../veyon-src   Veyon source tree (for core headers)
#   VEYON_CORE_LIBRARY   auto-selected  libveyon-core.dll.a (Qt5) or
#                                       libveyon-core-qt6.dll.a (Qt6)
#
cmake_minimum_required(VERSION 3.16)
project(InternetGuard LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Options / paths
# ---------------------------------------------------------------------------
option(WITH_QT6 "Build against Qt 6 (target Veyon built with WITH_QT6=ON)" OFF)

set(VEYON_TARGET_VERSION "4.7.5" CACHE STRING
    "Target Veyon version (e.g. 4.7.5, 4.9.8, 4.10.2)")

set(VEYON_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../veyon-src" CACHE PATH
    "Path to the Veyon source tree providing core/src headers")

if(WITH_QT6)
    set(_default_veyon_lib "${CMAKE_CURRENT_SOURCE_DIR}/libveyon-core-qt6.dll.a")
else()
    set(_default_veyon_lib "${CMAKE_CURRENT_SOURCE_DIR}/libveyon-core.dll.a")
endif()
set(VEYON_CORE_LIBRARY "${_default_veyon_lib}" CACHE FILEPATH
    "Path to the veyon-core import library to link against")

# Veyon core is compiled as C++14; match it so the ABI/headers agree.
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ---------------------------------------------------------------------------
# Decode VEYON_TARGET_VERSION into integer components for VeyonCompat.h
# ---------------------------------------------------------------------------
string(REPLACE "." ";" _veyon_ver_parts "${VEYON_TARGET_VERSION}")
list(LENGTH _veyon_ver_parts _veyon_ver_len)
list(GET _veyon_ver_parts 0 VEYON_VER_MAJOR)
if(_veyon_ver_len GREATER 1)
    list(GET _veyon_ver_parts 1 VEYON_VER_MINOR)
else()
    set(VEYON_VER_MINOR 0)
endif()
if(_veyon_ver_len GREATER 2)
    list(GET _veyon_ver_parts 2 VEYON_VER_PATCH)
else()
    set(VEYON_VER_PATCH 0)
endif()

# ---------------------------------------------------------------------------
# Qt
# ---------------------------------------------------------------------------
# Convenience: if Qt5 is not on the prefix path, fall back to the toolchain
# bundled with the assumed Qt 5.12.12 MinGW install. Override by passing
# -DCMAKE_PREFIX_PATH=... on the command line.
if(NOT WITH_QT6 AND NOT DEFINED Qt5_DIR
   AND NOT DEFINED CMAKE_PREFIX_PATH
   AND EXISTS "C:/Qt/5.12.12/mingw73_64")
    set(CMAKE_PREFIX_PATH "C:/Qt/5.12.12/mingw73_64")
endif()

if(WITH_QT6)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Svg Network)
    set(VEYON_QT_LIBS Qt6::Core Qt6::Widgets Qt6::Svg Qt6::Network)
    set(VEYON_QT_MAJOR 6)
else()
    find_package(Qt5 5.12 REQUIRED COMPONENTS Core Widgets Svg Network)
    set(VEYON_QT_LIBS Qt5::Core Qt5::Widgets Qt5::Svg Qt5::Network)
    set(VEYON_QT_MAJOR 5)
endif()

# ---------------------------------------------------------------------------
# Target
# ---------------------------------------------------------------------------
add_library(internet-guard SHARED
    InternetGuardPlugin.cpp
    InternetGuardPlugin.h
    VeyonCompat.h
    resources.qrc
)

target_include_directories(internet-guard PRIVATE
    "${VEYON_SOURCE_DIR}/core/src"
)

target_compile_definitions(internet-guard PRIVATE
    QT_DEPRECATED_WARNINGS
    VEYON_TARGET_VERSION_MAJOR=${VEYON_VER_MAJOR}
    VEYON_TARGET_VERSION_MINOR=${VEYON_VER_MINOR}
    VEYON_TARGET_VERSION_PATCH=${VEYON_VER_PATCH}
)

target_link_libraries(internet-guard PRIVATE
    ${VEYON_QT_LIBS}
    "${VEYON_CORE_LIBRARY}"
)

# GCC 16+ emits both a local vtable and __imp__ZTV references for dllimport base
# classes (FeatureProviderInterface, PluginInterface). The vtable definitions are
# identical (same header), so picking the local copy is safe.
if(WIN32)
    target_link_options(internet-guard PRIVATE -Wl,--allow-multiple-definition)
endif()

if(WITH_QT6)
    set(_output_name "internet-guard-qt6")
else()
    set(_output_name "internet-guard-qt5")
endif()
set_target_properties(internet-guard PROPERTIES
    PREFIX ""
    OUTPUT_NAME "${_output_name}"
)

message(STATUS "InternetGuard: target Veyon ${VEYON_TARGET_VERSION} (Qt${VEYON_QT_MAJOR})")
