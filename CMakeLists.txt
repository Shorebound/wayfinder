cmake_minimum_required(VERSION 4.0.1)

# --- Project Definition ---
project(wayfinder_project VERSION 0.1.0 LANGUAGES C CXX)
set_property(GLOBAL PROPERTY USE_FOLDERS ON) 
set(CMAKE_SUPPRESS_REGENERATION TRUE)

# --- Set Default Folder for Unspecified Targets ---
set(CMAKE_FOLDER "CMakeTargets" CACHE STRING "Default IDE folder for targets without explicit FOLDER property")
message(STATUS "Setting default target folder for unspecified targets to: ${CMAKE_FOLDER}")

set(ENGINE_NAME wayfinder)
set(EDITOR_NAME cartographer)
set(PROJECT_MANAGER_NAME compass)
set(DEBUGGER_SUITE_NAME surveyor)
set(BUILD_ORCHESTRATOR_NAME expedition)
set(DIAGNOSTICS_CONSOLE_NAME beacon)
set(ASSET_PROCESSOR_NAME waypoint)

set(HEADLESS_SERVER_NAME navigator)
set(ISSUE_TRACKER_NAME gh-issues)

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
set(ISSUE_TRACKER_PATH ${TOOLS_DIRECTORY}/${ISSUE_TRACKER_NAME})

set(SANDBOX_PATH ${SANDBOX_DIRECTORY}/${SANDBOX_NAME})
set(RUNTIME_PATH ${SANDBOX_DIRECTORY}/${RUNTIME_NAME})

# --- Global Settings ---
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON) 
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- Configurations ---
# Define the custom configurations. FORCE ensures this overwrites any
# value potentially set by a toolchain, preset, or previous cache.
set(CMAKE_CONFIGURATION_TYPES Debug Development Shipping CACHE STRING "Build types" FORCE)

# Set a default build type for single-config generators (Makefiles, Ninja)
# This part is still useful if someone uses a single-config generator.
if(NOT CMAKE_BUILD_TYPE AND CMAKE_CONFIGURATION_TYPES)
    # Set default CMAKE_BUILD_TYPE to the first type listed if none specified
    list(GET CMAKE_CONFIGURATION_TYPES 0 FIRST_CONFIG_TYPE)
    set(CMAKE_BUILD_TYPE ${FIRST_CONFIG_TYPE} CACHE STRING "Default build type" FORCE)
    # Or just hardcode Debug if you prefer:
    # set(CMAKE_BUILD_TYPE Debug CACHE STRING "Default build type" FORCE)
endif()
# --- Debug Messages (Optional but helpful) ---
message(STATUS "CMAKE_CONFIGURATION_TYPES forced to: ${CMAKE_CONFIGURATION_TYPES}")
message(STATUS "CMAKE_BUILD_TYPE default set to: ${CMAKE_BUILD_TYPE}")

# Enable LTO specifically for the Development & Shipping configurations
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_Shipping ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_Development ON)

# You can also set it per-target if needed:
# set_target_properties(${ENGINE_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_Shipping ON)
# set_target_properties(${SANDBOX_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_Shipping ON)

message(STATUS "Available configurations: ${CMAKE_CONFIGURATION_TYPES}")
message(STATUS "Building configuration: ${CMAKE_BUILD_TYPE}") 

# --- Project Settings ---
option(WAYFINDER_BUILD_EDITOR "Build the Wayfinder Editor" OFF)
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

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(WayfinderCommon)

# --- Output Directories ---
set(OUTPUT_DIR_BASE "${CMAKE_BINARY_DIR}/bin")

function(wayfinder_set_output_directories target_name)
  set_target_properties(${target_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR_BASE}/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR_BASE}/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR_BASE}/$<CONFIG>"
  )
endfunction()

if(WIN32)
  # Set the minimum Windows SDK version (adjust if needed)
  # This is often better handled by the toolchain/VS installation, but can be set.
  # set(CMAKE_SYSTEM_VERSION "10.0.17134.0")

  # Use dynamic runtime library (/MD or /MDd) — required for safe DLL boundary
  # with game modules (journey_module.dll etc.) loaded at runtime
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  message(STATUS "Windows: Using dynamic runtime library based on configuration.")

endif()

# --- Dependencies ---
include(WayfinderDependencies)

set(DEPS_FOLDER_NAME "ThirdParty")
set(SDL3_SUBFOLDER "${DEPS_FOLDER_NAME}/SDL3") 

if(TARGET SDL3-static)
    set_target_properties(SDL3-static PROPERTIES FOLDER ${SDL3_SUBFOLDER})
endif()
if(TARGET SDL3_Headers)
    set_target_properties(SDL3_Headers PROPERTIES FOLDER ${SDL3_SUBFOLDER})
endif()
if(TARGET glm)
    set_target_properties(glm PROPERTIES FOLDER ${DEPS_FOLDER_NAME})
endif()
if(TARGET spdlog) 
    set_target_properties(spdlog PROPERTIES FOLDER ${DEPS_FOLDER_NAME})
endif()
if(TARGET box2d) 
    set_target_properties(box2d PROPERTIES FOLDER ${DEPS_FOLDER_NAME})
endif()
if(TARGET tomlplusplus) 
    set_target_properties(tomlplusplus PROPERTIES FOLDER ${DEPS_FOLDER_NAME})
endif()

if(TARGET json) 
    set_target_properties(json PROPERTIES FOLDER ${DEPS_FOLDER_NAME})
endif()

if(TARGET tracy) 
    set_target_properties(tracy PROPERTIES FOLDER ${DEPS_FOLDER_NAME})
endif()

if (TARGET JoltPhysics)
    set_target_properties(JoltPhysics PROPERTIES FOLDER ${DEPS_FOLDER_NAME})
endif()

if (TARGET flecs)
    set_target_properties(flecs PROPERTIES FOLDER ${DEPS_FOLDER_NAME})
endif()

if(TARGET imgui) 
    set_target_properties(imgui PROPERTIES FOLDER ${DEPS_FOLDER_NAME})
endif()


add_subdirectory(${ENGINE_PATH})
set_target_properties(${ENGINE_NAME} PROPERTIES FOLDER "${ENGINE_DIRECTORY}") 

if(WAYFINDER_BUILD_EDITOR)
    #add_subdirectory(${EDITOR_PATH})
    #set_target_properties(${EDITOR_NAME} PROPERTIES FOLDER "${EDITOR_DIRECTORY}") 
endif()

if(WAYFINDER_BUILD_SANDBOX)
    add_subdirectory(${SANDBOX_PATH}) 
    set_target_properties(${SANDBOX_NAME} PROPERTIES FOLDER "${SANDBOX_DIRECTORY}")
endif()

if(WAYFINDER_BUILD_RUNTIME)
    #add_subdirectory(${RUNTIME_PATH})
endif()

if(WAYFINDER_BUILD_TOOLS)
    add_subdirectory(${ASSET_PROCESSOR_PATH})
    set_target_properties(${ASSET_PROCESSOR_NAME} PROPERTIES FOLDER "${TOOLS_DIRECTORY}")

    add_subdirectory(${ISSUE_TRACKER_PATH})
    set_target_properties(${ISSUE_TRACKER_NAME} PROPERTIES FOLDER "${TOOLS_DIRECTORY}")
endif()

if(WAYFINDER_BUILD_SAMPLES)
    #add_subdirectory(${SAMPLES_NAME})
endif()

if(WAYFINDER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(${TESTS_DIRECTORY})
endif()

message(STATUS "Configuration complete.")
