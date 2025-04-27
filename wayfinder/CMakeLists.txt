if(WAYFINDER_BUILD_SHARED_LIBS)
  set(ENGINE_LIB_TYPE SHARED)
else()
  set(ENGINE_LIB_TYPE STATIC)
endif()

#set(ENGINE_NAME wayfinder)

add_library(${ENGINE_NAME} ${ENGINE_LIB_TYPE}) 
message(STATUS "Engine: Configuring target ${ENGINE_NAME} (${ENGINE_LIB_TYPE})")

include(GenerateExportHeader)
if(WIN32 AND ENGINE_LIB_TYPE STREQUAL "SHARED")
  # Define WAYFINDER_EXPORTS *only* when building this DLL itself
  target_compile_definitions(${ENGINE_NAME} PRIVATE WAYFINDER_EXPORTS)
  message(STATUS "Engine: Enabling DLL export definitions for ${ENGINE_NAME}")
endif()

# Generate Wayfinder_export.h in the build directory's include folder
# This header defines the WAYFINDER_API macro (__declspec(dllexport/dllimport) or nothing)
generate_export_header(${ENGINE_NAME}
    BASE_NAME Wayfinder # -> Wayfinder_export.h, defines WAYFINDER_API
    EXPORT_MACRO_NAME WAYFINDER_API
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/Wayfinder_export.h
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

# Our Project
target_sources(${ENGINE_NAME} PRIVATE
    #src/main.cpp
    src/core/Application.cpp
    src/core/Game.cpp
    src/rendering/Renderer.cpp
    src/scene/Scene.cpp
    src/input/InputManager.cpp
    src/scene/entity/Entity.cpp
    src/scene/entity/Component.cpp
    src/scene/entity/Transform.cpp
)

# Include directories
#target_include_directories(${ENGINE_NAME} PRIVATE
#    src
#)

target_include_directories(${ENGINE_NAME} PUBLIC
    # PUBLIC: Needed by Wayfinder itself AND consumers (like waystone)
    # Allows #include "core/Application.h", etc.
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    # PUBLIC: Needed by consumers to find the generated Wayfinder_export.h
    # $<BUILD_INTERFACE:...> ensures this path is only added for consumers
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
)

# --- Compile Definitions ---
# Define flags needed when compiling Wayfinder OR when compiling consumers
target_compile_definitions(${ENGINE_NAME} PUBLIC
    # Example: Allow consumers and Wayfinder itself to check if logging is on
    WAYFINDER_LOGGING_ENABLED
    # Add other PUBLIC definitions needed by headers in src/ here
)
# Add PRIVATE definitions only needed inside Wayfinder's .cpp files if necessary
# target_compile_definitions(${ENGINE_NAME} PRIVATE INTERNAL_ENGINE_FLAG)



#set(raylib_VERBOSE 1)
target_link_libraries(${ENGINE_NAME} PUBLIC raylib)

target_compile_definitions(${ENGINE_NAME}
    PRIVATE
        WAYFINDER_CORE_BUILD # Define this only when building core itself
    PUBLIC
        # If building shared libs on Windows, we need to handle export/import
        $<$<AND:$<BOOL:${WAYFINDER_BUILD_SHARED_LIBS}>,$<PLATFORM_ID:Windows>>:WAYFINDER_API_EXPORT>
        # Define a feature flag that consumers might need
        WAYFINDER_LOGGING_ENABLED
)
