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

# --- Compile Definitions ---
target_compile_definitions(${ENGINE_NAME} 
  PUBLIC
    # Allow consumers and Wayfinder itself to check if logging is on
    WAYFINDER_LOGGING_ENABLED

    $<$<CONFIG:Debug>:WAYFINDER_DEBUG>
    $<$<CONFIG:Development>:WAYFINDER_DEVELOPMENT> 
    $<$<CONFIG:Shipping>:WAYFINDER_SHIPPING>    

    # Config-specific logging levels
    $<$<CONFIG:Debug>:WAYFINDER_LOGGING_LEVEL=0>
    $<$<CONFIG:Development>:WAYFINDER_LOGGING_LEVEL=1>
    $<$<CONFIG:Shipping>:WAYFINDER_LOGGING_LEVEL=2>

    # Platform-specific (Public controls #ifdef) - uses generator expression
    $<IF:$<PLATFORM_ID:Windows>,WAYFINDER_PLATFORM_WINDOWS,> # Add if Windows, else add nothing
    
  PRIVATE
    # Platform-specific (Private controls __declspec) - uses generator expression
    $<IF:$<PLATFORM_ID:Windows>,WAYFINDER_BUILD_DLL,> # Add if Windows, else add nothing
)

# --- Sources ---
file(GLOB_RECURSE ${ENGINE_NAME}_SOURCES CONFIGURE_DEPENDS
  "src/*.h"
  "src/*.cpp"
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Core.h
    DESTINATION include/wayfinder/core
)

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

# --- Target Properties ---
# Output directory for the library file (.dll, .lib)
set_target_properties(${ENGINE_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR_BASE}/$<CONFIG>" # For DLL
  LIBRARY_OUTPUT_DIRECTORY "${OUTPUT_DIR_BASE}/$<CONFIG>" # For import lib
  ARCHIVE_OUTPUT_DIRECTORY "${OUTPUT_DIR_BASE}/$<CONFIG>" # For static lib (if type changed)
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

# Compiler Flags (Symbols / Optimization)
# CMake handles debug symbols (-g / /Zi) for Debug builds automatically.
# CMake handles optimization (-O2, -O3 / /O2) for Release-like builds automatically.
# We rely on CMake's defaults associated with the build type/configuration.
# If more specific flags are needed, use target_compile_options with generator expressions:
# target_compile_options(${ENGINE_NAME} PRIVATE
#   $<$<CONFIG:Debug>:/Zi> # Example: Force specific debug info format
#   $<$<CONFIG:Development>:/O2>
#   $<$<CONFIG:Shipping>:/O2;/GL> # Example: Add Link-Time Optimization for Shipping
# )

message(STATUS "Engine: Configured precompiled headers for ${ENGINE_NAME}")