cmake_minimum_required(VERSION 3.26.0)

# --- Project Definition ---
project(wayfinder_project VERSION 0.1.0 LANGUAGES C CXX)
set(ENGINE_NAME wayfinder)
set(RUNTIME_NAME beacon)
set(EDITOR_NAME compass)
set(SANDBOX_NAME waystone)
set(TOOLS_NAME navigator)
set(ASSET_PROCESSOR_NAME cartographer)
set(SAMPLES_NAME journey)
set(TESTS_NAME surveyor)

# --- Global Settings ---
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Configurations ---
# Define the custom configurations if none are specified (e.g., by IDE/Generator)
# This ensures they are available, especially for multi-config generators like Visual Studio.
if(NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES Debug Development Shipping CACHE STRING "Build types" FORCE)
  # Set a default build type for single-config generators (Makefiles, Ninja)
  # if the user doesn't specify one (-DCMAKE_BUILD_TYPE=...)
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Default build type" FORCE)
  endif()
endif()
message(STATUS "Available configurations: ${CMAKE_CONFIGURATION_TYPES}")
message(STATUS "Building configuration: ${CMAKE_BUILD_TYPE}") # Empty for multi-config

# --- Project Settings ---
option(WAYFINDER_BUILD_EDITOR "Build the Wayfinder Editor" ON)
option(WAYFINDER_BUILD_RUNTIME "Build the Wayfinder Runtime" OFF)
option(WAYFINDER_BUILD_SANDBOX "Build the Wayfinder Sandbox" ON)
option(WAYFINDER_BUILD_TOOLS "Build standalone tools" OFF)
option(WAYFINDER_BUILD_SAMPLES "Build sample projects" OFF)
option(WAYFINDER_BUILD_TESTS "Build tests" OFF) 
option(WAYFINDER_BUILD_ASSETPROCESSOR "Build the Wayfinder Asset Processor" OFF)
option(WAYFINDER_BUILD_SHARED_LIBS "Build engine modules as shared libraries" ON)

message(STATUS "Wayfinder Project Configuration:")
message(STATUS "  Build Sandbox (Waystone): ${WAYFINDER_BUILD_SANDBOX}")
message(STATUS "  Build Editor (Compass): ${WAYFINDER_BUILD_EDITOR}")
message(STATUS "  Build Runtime (Beacon): ${WAYFINDER_BUILD_RUNTIME}")
message(STATUS "  Build Tools (Navigator): ${WAYFINDER_BUILD_TOOLS}")
message(STATUS "  Build Samples (Journey): ${WAYFINDER_BUILD_SAMPLES}")
message(STATUS "  Build Tests (Surveyor): ${WAYFINDER_BUILD_TESTS}")
message(STATUS "  Shared Libs: ${WAYFINDER_BUILD_SHARED_LIBS}")

# --- Output Directories ---
# Define a base directory for output binaries relative to the build directory
set(OUTPUT_DIR_BASE "${CMAKE_BINARY_DIR}/bin")
# Note: Per-target output directories are set within each sub-project below
# using generator expressions like $<CONFIG> to handle configuration names.

if(WIN32)
  # Set the minimum Windows SDK version (adjust if needed)
  # This is often better handled by the toolchain/VS installation, but can be set.
  # set(CMAKE_SYSTEM_VERSION "10.0.17134.0")

  # Use static runtime library (/MT or /MTd)
  # Use a generator expression to select based on configuration
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  message(STATUS "Windows: Using static runtime library based on configuration.")
endif()

# --- Dependencies ---
set(RAYLIB_VERSION 5.5)
find_package(raylib ${RAYLIB_VERSION} QUIET) # QUIET or REQUIRED
if (NOT raylib_FOUND) # If there's none, fetch and build raylib
  include(FetchContent)
  FetchContent_Declare(
    raylib
    DOWNLOAD_EXTRACT_TIMESTAMP OFF
    URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
  )
  FetchContent_GetProperties(raylib)
  if (NOT raylib_POPULATED) # Have we downloaded raylib yet?
    set(FETCHCONTENT_QUIET NO)
    set(BUILD_EXAMPLES OFF CACHE BOOL "Build raylib examples" FORCE)
    #set(BUILD_GAMES OFF CACHE BOOL "Build raylib games" FORCE)
    #set(BUILD_TESTS OFF CACHE BOOL "Build raylib tests" FORCE)
    FetchContent_MakeAvailable(raylib)
  endif()
endif()

# Logging Library (spdlog)
set(SPDLOG_VERSION 1.15.2)
find_package(spdlog ${SPDLOG_VERSION} QUIET)
if (NOT spdlog_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        spdlog
        DOWNLOAD_EXTRACT_TIMESTAMP OFF
        URL https://github.com/gabime/spdlog/archive/refs/tags/v${SPDLOG_VERSION}.tar.gz
    )
    FetchContent_MakeAvailable(spdlog)
endif()

add_subdirectory(${ENGINE_NAME})

if(WAYFINDER_BUILD_SANDBOX)
    add_subdirectory(${SANDBOX_NAME}) 
endif()

if(WAYFINDER_BUILD_RUNTIME)
    #add_subdirectory(${RUNTIME_NAME})
endif()

if(WAYFINDER_BUILD_EDITOR)
    #add_subdirectory(${EDITOR_NAME})
endif()

if(WAYFINDER_BUILD_RUNTIME)
    #add_subdirectory(${RUNTIME_NAME})
endif()

if(WAYFINDER_BUILD_TOOLS)
    #add_subdirectory(${TOOLS_NAME})
endif()

if(WAYFINDER_BUILD_ASSETPROCESSOR)
    #add_subdirectory(${ASSET_PROCESSOR_NAME})
endif()

if(WAYFINDER_BUILD_SAMPLES)
    #add_subdirectory(${SAMPLES_NAME})
endif()

if(WAYFINDER_BUILD_TESTS)
    #enable_testing()
    #add_subdirectory(${TESTS_NAME})
endif()

message(STATUS "Configuration complete.")