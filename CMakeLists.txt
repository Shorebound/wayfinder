cmake_minimum_required(VERSION 4.0.1)

# --- Project Definition ---
project(wayfinder_project VERSION 0.1.0 LANGUAGES C CXX)

set(ENGINE_NAME wayfinder)
set(EDITOR_NAME cartographer)
set(PROJECT_MANAGER_NAME compass)
set(DEBUGGER_SUITE_NAME surveyor)
set(BUILD_ORCHESTRATOR_NAME expedition)
set(DIAGNOSTICS_CONSOLE_NAME beacon)
set(ASSET_PROCESSOR_NAME waypoint)

set(HEADLESS_SERVER_NAME navigator)

set(SANDBOX_NAME journey)
set(RUNTIME_NAME waystone)

set(ENGINE_DIRECTORY engine)
set(APPS_DIRECTORY apps)
set(SAMPLES_DIRECTORY samples)
set(SANDBOX_DIRECTORY sandbox)
set(TOOLS_DIRECTORY tools)
set(GAMES_DIRECTORY games)
set(TESTS_DIRECTORY tests)

set(ENGINE_PATH ${ENGINE_DIRECTORY}/${ENGINE_NAME})
set(EDITOR_PATH ${APPS_DIRECTORY}/${EDITOR_NAME})
set(PROJECT_MANAGER_PATH ${APPS_DIRECTORY}/${PROJECT_MANAGER_NAME})
set(DEBUGGER_SUITE_PATH ${TOOLS_DIRECTORY}/${DEBUGGER_SUITE_NAME})
set(BUILD_ORCHESTRATOR_PATH ${TOOLS_DIRECTORY}/${BUILD_ORCHESTRATOR_NAME})
set(DIAGNOSTICS_CONSOLE_PATH ${TOOLS_DIRECTORY}/${DIAGNOSTICS_CONSOLE_NAME})
set(ASSET_PROCESSOR_PATH ${TOOLS_DIRECTORY}/${ASSET_PROCESSOR_NAME})

set(HEADLESS_SERVER_PATH ${TOOLS_DIRECTORY}/${HEADLESS_SERVER_NAME})

set(SANDBOX_PATH ${SANDBOX_DIRECTORY}/${SANDBOX_NAME})
set(RUNTIME_PATH ${SANDBOX_DIRECTORY}/${RUNTIME_NAME})

# --- Global Settings ---
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON) 

# set(CMAKE_FOLDER_NAME "_CMakeTargets")
# Assign standard targets to the folder
# Note: ZERO_CHECK might be hidden by CMAKE_SUPPRESS_REGENERATION later
# set_target_properties(ALL_BUILD INSTALL PACKAGE PROPERTIES FOLDER ${CMAKE_FOLDER_NAME})
# If you see ZERO_CHECK and want to group it:
# set_target_properties(ZERO_CHECK PROPERTIES FOLDER ${CMAKE_FOLDER_NAME})
# Alternatively, hide ZERO_CHECK completely (often preferred):
# set(CMAKE_SUPPRESS_REGENERATION TRUE)

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

# Enable LTO specifically for the Development & Shipping configurations
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_Shipping ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_Development ON)

# You can also set it per-target if needed:
# set_target_properties(${ENGINE_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_Shipping ON)
# set_target_properties(${SANDBOX_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_Shipping ON)

message(STATUS "Available configurations: ${CMAKE_CONFIGURATION_TYPES}")
message(STATUS "Building configuration: ${CMAKE_BUILD_TYPE}") 

# --- Project Settings ---
option(WAYFINDER_BUILD_EDITOR "Build the Wayfinder Editor" ON)
option(WAYFINDER_BUILD_RUNTIME "Build the Wayfinder Runtime" OFF)
option(WAYFINDER_BUILD_SANDBOX "Build the Wayfinder Sandbox" ON)
option(WAYFINDER_BUILD_TOOLS "Build standalone tools" OFF)
option(WAYFINDER_BUILD_SAMPLES "Build sample projects" OFF)
option(WAYFINDER_BUILD_TESTS "Build tests" OFF) 
option(WAYFINDER_BUILD_SHARED_LIBS "Build engine modules as shared libraries" OFF)

option(WAYFINDER_ENABLE_RTTI "Enable C++ Run-Time Type Information" ON)
option(WAYFINDER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(WAYFINDER_ENABLE_EXCEPTIONS "Enable C++ Exceptions" ON)
option(WAYFINDER_ENABLE_LOGGING "Enable the logging system" ON) 

option(WAYFINDER_PHYSICS "Enable physics support" ON)

message(STATUS "Wayfinder Project Configuration:")
message(STATUS "  Build Sandbox: ${WAYFINDER_BUILD_SANDBOX}")
message(STATUS "  Build Editor: ${WAYFINDER_BUILD_EDITOR}")
message(STATUS "  Build Runtime: ${WAYFINDER_BUILD_RUNTIME}")
message(STATUS "  Build Tools: ${WAYFINDER_BUILD_TOOLS}")
message(STATUS "  Build Samples: ${WAYFINDER_BUILD_SAMPLES}")
message(STATUS "  Build Tests: ${WAYFINDER_BUILD_TESTS}")
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

  # Use dynamic runtime library (/MD or /MDd)
  # set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  # message(STATUS "Windows: Using dynamic runtime library based on configuration.")

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

add_subdirectory(${ENGINE_PATH})
if(WAYFINDER_BUILD_EDITOR)
    #add_subdirectory(${EDITOR_PATH})
endif()

if(WAYFINDER_BUILD_SANDBOX)
    add_subdirectory(${SANDBOX_PATH}) 
endif()

if(WAYFINDER_BUILD_RUNTIME)
    #add_subdirectory(${RUNTIME_PATH})
endif()

if(WAYFINDER_BUILD_TOOLS)
    #add_subdirectory(${TOOLS_NAME})
endif()

if(WAYFINDER_BUILD_SAMPLES)
    #add_subdirectory(${SAMPLES_NAME})
endif()

if(WAYFINDER_BUILD_TESTS)
    #enable_testing()
    #add_subdirectory(${TESTS_NAME})
endif()

message(STATUS "Configuration complete.")