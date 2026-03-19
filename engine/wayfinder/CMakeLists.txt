if(WAYFINDER_BUILD_SHARED_LIBS)
  set(ENGINE_LIB_TYPE SHARED)
else()
  set(ENGINE_LIB_TYPE STATIC)
endif()

set(ENGINE_EXPORT_NAME WAYFINDER)

add_library(${ENGINE_NAME} ${ENGINE_LIB_TYPE})
wayfinder_set_output_directories(${ENGINE_NAME})
message(STATUS "Engine: Configuring target ${ENGINE_NAME} (${ENGINE_LIB_TYPE})")

target_compile_definitions(${ENGINE_NAME}
  PRIVATE # Only needed when building the engine itself
    # Platform-specific DLL export control (only if building DLL)
    $<IF:$<BOOL:${WAYFINDER_BUILD_SHARED_LIBS}>,$<IF:$<PLATFORM_ID:Windows>,WAYFINDER_BUILD_DLL,>,>
    # Add any other definitions ONLY needed privately by the engine
)

include(GenerateExportHeader)
if(WIN32 AND WAYFINDER_BUILD_SHARED_LIBS)
  # Define WAYFINDER_EXPORTS *only* when building this DLL itself
  target_compile_definitions(${ENGINE_NAME} PRIVATE WAYFINDER_EXPORTS)
  message(STATUS "Engine: Enabling DLL export definitions for ${ENGINE_NAME}")
endif()

# Generate wayfinder_exports.h in the build directory's include folder
# This header defines the WAYFINDER_API macro (__declspec(dllexport/dllimport) or nothing)
generate_export_header(${ENGINE_NAME}
    BASE_NAME ${ENGINE_EXPORT_NAME}
    EXPORT_MACRO_NAME ${ENGINE_EXPORT_NAME}_API
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/wayfinder_exports.h
)

# --- Sources ---
file(GLOB_RECURSE ENGINE_ALL_SOURCES CONFIGURE_DEPENDS
  "src/*.h"
  "src/*.cpp"
)

# glob for now
target_sources(${ENGINE_NAME} PRIVATE ${ENGINE_ALL_SOURCES})

foreach(FILEPATH ${ENGINE_ALL_SOURCES})
    # Get the path relative to src/
    file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src" "${FILEPATH}")
    # Get the directory part
    get_filename_component(PARENT_DIR "${REL_PATH}" DIRECTORY)
    # Replace / with \\ for Visual Studio filters
    string(REPLACE "/" "\\" GROUP_PATH "${PARENT_DIR}")
    # Create the group (CMake handles duplicates)
    if(GROUP_PATH)
      source_group("${GROUP_PATH}" FILES "${FILEPATH}")
    else()
      # Files directly under src/
      source_group("Root" FILES "${FILEPATH}")
    endif()
endforeach()

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/Core.h
    DESTINATION include/wayfinder/core
)

# Include directories
target_include_directories(${ENGINE_NAME}
  PUBLIC
    # PUBLIC: Needed by Wayfinder itself AND consumers (like waystone)
    # Allows #include "core/Application.h", etc.
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    # PUBLIC: Needed by consumers to find the generated Wayfinder_export.h
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
)

target_link_libraries(${ENGINE_NAME}
    PUBLIC
      wayfinder_common
      SDL3::SDL3
      glm::glm
      flecs::flecs_static
  tomlplusplus::tomlplusplus
)

if(WAYFINDER_ENABLE_LOGGING)
    target_link_libraries(${ENGINE_NAME} PUBLIC spdlog::spdlog)
    target_sources(${ENGINE_NAME} 
    PRIVATE 
      src/core/Log.cpp
      src/core/logging/spdlog/SpdLogOutput.cpp
    )
endif()

# Configure precompiled headers
target_precompile_headers(${ENGINE_NAME}
    PRIVATE
        src/WayfinderPCH.h
)

message(STATUS "Engine: Configured precompiled headers for ${ENGINE_NAME}")
#   $<$<CONFIG:Development>:/O2>
#   $<$<CONFIG:Shipping>:/O2;/GL> # Example: Add Link-Time Optimization for Shipping
# )

message(STATUS "Engine: Configured precompiled headers for ${ENGINE_NAME}")