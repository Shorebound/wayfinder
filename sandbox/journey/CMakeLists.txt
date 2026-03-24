# journey/CMakeLists.txt

# Define the executable target
add_executable(${SANDBOX_NAME})
wayfinder_set_output_directories(${SANDBOX_NAME})
message(STATUS "Sandbox: Configuring target ${SANDBOX_NAME}")

# --- Sources ---
target_sources(${SANDBOX_NAME} 
    PRIVATE
        src/JourneyGame.cpp 
)

# --- Include Directories ---
target_include_directories(${SANDBOX_NAME} 
    PRIVATE
        src
)

# --- Link Libraries ---
target_link_libraries(${SANDBOX_NAME} 
    PRIVATE
        ${ENGINE_NAME}
)

# --- Game plugin shared library (loaded at runtime by tools such as waypoint) ---
set(GAME_PLUGIN_LIB ${SANDBOX_NAME}_plugin)
add_library(${GAME_PLUGIN_LIB} SHARED src/JourneyGame.cpp)
wayfinder_set_output_directories(${GAME_PLUGIN_LIB})
target_include_directories(${GAME_PLUGIN_LIB} PRIVATE src)
target_link_libraries(${GAME_PLUGIN_LIB} PRIVATE ${ENGINE_NAME})

add_custom_command(TARGET ${SANDBOX_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:${SANDBOX_NAME}>/assets"
  COMMENT "Copying sandbox assets for ${SANDBOX_NAME}"
  VERBATIM
)

add_custom_command(TARGET ${SANDBOX_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/project.wayfinder"
    "$<TARGET_FILE_DIR:${SANDBOX_NAME}>/project.wayfinder"
  COMMENT "Copying project.wayfinder for ${SANDBOX_NAME}"
  VERBATIM
)

add_custom_command(TARGET ${SANDBOX_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/config"
    "$<TARGET_FILE_DIR:${SANDBOX_NAME}>/config"
  COMMENT "Copying config directory for ${SANDBOX_NAME}"
  VERBATIM
)

# --- Shader Compilation ---
include(WayfinderShaders)
file(GLOB SHADER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/engine/wayfinder/shaders/*.vert"
    "${CMAKE_SOURCE_DIR}/engine/wayfinder/shaders/*.frag"
)
wayfinder_compile_shaders(
    TARGET ${SANDBOX_NAME}
    SHADERS ${SHADER_SOURCES}
    OUTPUT_DIR "$<TARGET_FILE_DIR:${SANDBOX_NAME}>/assets/shaders"
)

# --- Platform Specifics for THIS Executable ---
# Web Configurations (using standard CMake variable)
if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    message(STATUS "Sandbox: Configuring ${SANDBOX_NAME} for Web (Emscripten)")
    set_target_properties(${SANDBOX_NAME} PROPERTIES SUFFIX ".html")
    # Use target_link_options (CMake >= 3.13) for cleaner linker flags
    target_link_options(${SANDBOX_NAME} PRIVATE
        "-s USE_GLFW=3"
        "-s ASSERTIONS=1"
        "-s WASM=1"
        "-s ASYNCIFY"
        "-s GL_ENABLE_GET_PROC_ADDRESS=1"
        # Add other Emscripten flags needed by the sandbox/game
    )
endif()

# Apple Framework Linking
if (APPLE)
    message(STATUS "Sandbox: Linking Apple Frameworks for ${SANDBOX_NAME}")
    target_link_libraries(${SANDBOX_NAME} PRIVATE
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
        # Add other frameworks needed by the sandbox/game
    )
endif()

# --- Post-Build Command for Sandbox (Windows Only) ---
# Sandbox copies the Wayfinder DLL it needs into its own output directory.
if(WIN32 AND WAYFINDER_BUILD_SHARED_LIBS)
  # Check if Wayfinder is actually being built as a shared library
  # Get the TYPE property of the Wayfinder target
  get_target_property(WAYFINDER_TYPE ${ENGINE_NAME} TYPE)
  if(WAYFINDER_TYPE STREQUAL "SHARED_LIBRARY")

    add_custom_command(TARGET ${SANDBOX_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        # Source: The Wayfinder library file for the current configuration
        $<TARGET_FILE:${ENGINE_NAME}>
        # Destination: Sandbox's output directory
        "$<TARGET_FILE_DIR:${SANDBOX_NAME}>/"
      COMMENT "Copying ${ENGINE_NAME}.dll to ${SANDBOX_NAME} output directory for $<CONFIG>"
      VERBATIM
    )
    message(STATUS "Windows: Added post-build step for ${SANDBOX_NAME} to copy required ${ENGINE_NAME} DLL.")

  endif()
endif()