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

    # Config-specific definitions
    $<$<CONFIG:Debug>:WAYFINDER_DEBUG>
    $<$<CONFIG:Development>:WAYFINDER_DEVELOPMENT>
    $<$<CONFIG:Shipping>:WAYFINDER_SHIPPING>

    # Config-specific logging levels
    $<$<CONFIG:Debug>:WAYFINDER_LOGGING_LEVEL=0>
    $<$<CONFIG:Development>:WAYFINDER_LOGGING_LEVEL=1>
    $<$<CONFIG:Shipping>:WAYFINDER_LOGGING_LEVEL=2>

    # Debug
    $<$<NOT:$<CONFIG:Shipping>>:WAYFINDER_ENABLE_ASSERTS>
    $<$<BOOL:${WAYFINDER_ENABLE_LOGGING}>:WAYFINDER_LOGGING_ENABLED>

    # Config-specific Logging


    # Platform-specific (Public controls #ifdef) - uses generator expression
    $<IF:$<PLATFORM_ID:Windows>,WAYFINDER_PLATFORM_WINDOWS,>
    $<IF:$<PLATFORM_ID:Linux>,WAYFINDER_PLATFORM_LINUX,>
    $<IF:$<PLATFORM_ID:Darwin>,WAYFINDER_PLATFORM_MACOS,>
    # Add others for Android, iOS (using CMAKE_SYSTEM_NAME checks), Emscripten
    $<IF:$<STREQUAL:${CMAKE_SYSTEM_NAME},Emscripten>,WAYFINDER_PLATFORM_WEB,>

    # physics
    $<$<BOOL:${WAYFINDER_PHYSICS}>:WAYFINDER_PHYSICS>

  PRIVATE
    # Platform-specific (Private controls __declspec) - uses generator expression
    $<IF:$<PLATFORM_ID:Windows>,WAYFINDER_BUILD_DLL,> # Add if Windows, else add nothing

    $<IF:$<CXX_COMPILER_ID:MSVC>,WAYFINDER_COMPILER_MSVC,>
    $<IF:$<CXX_COMPILER_ID:GNU>,WAYFINDER_COMPILER_GCC,>
    $<IF:$<CXX_COMPILER_ID:Clang>,WAYFINDER_COMPILER_CLANG,>
    $<IF:$<CXX_COMPILER_ID:AppleClang>,WAYFINDER_COMPILER_CLANG,>
    $<IF:$<CXX_COMPILER_ID:Intel>,WAYFINDER_COMPILER_INTEL,>

)

# --- Compile Options ---
target_compile_options(${ENGINE_NAME} PRIVATE
    # Common flags for GCC/Clang
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
    # Consider adding more specific warnings for GCC/Clang if desired:
    # -Wshadow -Wconversion -Wsign-conversion -Wundef -Wunused-variable ...

    # Common flags for MSVC
    $<$<CXX_COMPILER_ID:MSVC>:/W4> # High warning level
    # Consider /permissive- for stricter standard conformance on MSVC
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
    # Consider /utf-8 to ensure source code is treated as UTF-8 on MSVC
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>

    # Add other options like RTTI/Exception control flags here if needed
)

# Optional: Treat warnings as errors (good for CI or strict development)
if(WAYFINDER_WARNINGS_AS_ERRORS)
    target_compile_options(${ENGINE_NAME} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Werror>
        $<$<CXX_COMPILER_ID:MSVC>:/WX>
    )
    message(STATUS "Treating compiler warnings as errors")
endif()

if(NOT WAYFINDER_ENABLE_RTTI)
    message(STATUS "Disabling RTTI")
    target_compile_options(${ENGINE_NAME} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/GR->
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fno-rtti>
    )
    # Apply to other targets (Sandbox, Editor, etc.) as needed
endif()

if(NOT WAYFINDER_ENABLE_EXCEPTIONS)
    message(STATUS "Disabling Exceptions")
    target_compile_options(${ENGINE_NAME} PRIVATE
        # MSVC: /EHs /EHc flags control exceptions. /EHs-c- might be needed. Check docs.
        # This is complex on MSVC, might need more specific flags depending on model.
        # A common starting point is disabling synchronous exceptions:
        $<$<CXX_COMPILER_ID:MSVC>:/EHs-c-> # Check MSVC docs for exact flags needed
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-fno-exceptions>
    )
    # Apply to other targets (Sandbox, Editor, etc.) as needed
endif()

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

    # Core
    src/core/Application.cpp
    src/core/Game.cpp
    src/core/Log.cpp
    src/core/ServiceLocator.cpp

    # Platform
    src/platform/raylib/RaylibWindow.cpp
    src/platform/raylib/RaylibInput.cpp
    src/platform/raylib/RaylibTime.cpp

    # Rendering
    src/rendering/Renderer.cpp
    src/rendering/raylib/RaylibGraphicsContext.cpp
    src/rendering/raylib/RaylibRenderAPI.cpp

    # Scene
    src/scene/Scene.cpp
    src/scene/entity/Entity.cpp
    src/scene/entity/Component.cpp
    src/scene/entity/Transform.cpp

    # Input
    src/input/InputManager.cpp
)



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

if(WAYFINDER_ENABLE_LOGGING)
    target_link_libraries(${ENGINE_NAME} PUBLIC spdlog::spdlog)
    # Include logging system
    add_subdirectory(src/core/logging)
endif()

target_link_libraries(${ENGINE_NAME}
    PUBLIC
        raylib
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