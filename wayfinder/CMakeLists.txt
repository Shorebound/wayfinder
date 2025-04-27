if(WAYFINDER_BUILD_SHARED_LIBS)
  set(ENGINE_LIB_TYPE SHARED)
else()
  set(ENGINE_LIB_TYPE STATIC)
endif()

add_library(${ENGINE_NAME} ${ENGINE_LIB_TYPE})
message(STATUS "Engine: Configuring target ${ENGINE_NAME} (${ENGINE_LIB_TYPE})")

include(GenerateExportHeader)

# Generate Wayfinder_export.h in the build directory's include folder
# This header defines the WAYFINDER_API macro (__declspec(dllexport/dllimport) or nothing)
generate_export_header(${ENGINE_NAME}
    BASE_NAME Wayfinder # -> Wayfinder_export.h, defines WAYFINDER_API
    EXPORT_MACRO_NAME WAYFINDER_API
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/Wayfinder_export.h
)

if(WIN32 AND ENGINE_LIB_TYPE STREQUAL "SHARED")
  # Define WAYFINDER_EXPORTS *only* when building this DLL itself
  target_compile_definitions(${ENGINE_NAME} PRIVATE WAYFINDER_EXPORTS)
  message(STATUS "Engine: Enabling DLL export definitions for ${ENGINE_NAME}")
endif()

# --- Compile Definitions ---
target_compile_definitions(${ENGINE_NAME} PUBLIC
    # Allow consumers and Wayfinder itself to check if logging is on
    WAYFINDER_LOGGING_ENABLED
    # Config-specific logging levels
    $<$<CONFIG:Debug>:WAYFINDER_LOGGING_LEVEL=0>
    $<$<CONFIG:Release>:WAYFINDER_LOGGING_LEVEL=2>
    # Add other PUBLIC definitions needed by headers in src/ here
)
# Add PRIVATE definitions only needed inside Wayfinder's .cpp files if necessary
# target_compile_definitions(${ENGINE_NAME} PRIVATE INTERNAL_ENGINE_FLAG)




# Install headers
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Core.h
    DESTINATION include/wayfinder/core
)

# Dependencies
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

# Our Project
target_sources(${ENGINE_NAME} PRIVATE
    # Precompiled Header
    src/WayfinderPCH.cpp

    src/core/Application.cpp
    src/core/Game.cpp
    src/core/Log.cpp
    src/rendering/Renderer.cpp
    src/scene/Scene.cpp
    src/input/InputManager.cpp
    src/scene/entity/Entity.cpp
    src/scene/entity/Component.cpp
    src/scene/entity/Transform.cpp
)

# Include logging system
add_subdirectory(src/core/logging)

# Include directories
target_include_directories(${ENGINE_NAME} 
  PUBLIC
    # PUBLIC: Needed by Wayfinder itself AND consumers (like waystone)
    # Allows #include "core/Application.h", etc.
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    # PUBLIC: Needed by consumers to find the generated Wayfinder_export.h
    # $<BUILD_INTERFACE:...> ensures this path is only added for consumers
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>

  PRIVATE
    # Needed by Wayfinder itself to find Wayfinder_export.h for its PCH
    ${CMAKE_CURRENT_BINARY_DIR}/include
)


#set(raylib_VERBOSE 1)
target_link_libraries(${ENGINE_NAME}
    PUBLIC
        raylib
        spdlog::spdlog
)

# Configure precompiled headers
target_precompile_headers(${ENGINE_NAME}
    PUBLIC
        src/WayfinderPCH.h
)

message(STATUS "Engine: Configured precompiled headers for ${ENGINE_NAME}")

