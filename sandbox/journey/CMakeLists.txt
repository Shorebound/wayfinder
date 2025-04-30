# journey/CMakeLists.txt

# Define the executable target
add_executable(${SANDBOX_NAME})
wayfinder_set_output_directories(${SANDBOX_NAME})
message(STATUS "Sandbox: Configuring target ${SANDBOX_NAME}")

# --- Installation ---
#install(TARGETS ${SANDBOX_NAME}
#    RUNTIME DESTINATION bin 
#)

# --- Sources ---
file(GLOB_RECURSE SANDBOX_SOURCES CONFIGURE_DEPENDS
  "src/*.h"
  "src/*.cpp"
)
target_sources(${SANDBOX_NAME} 
    PRIVATE
        src/JourneyApplication.cpp 
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

# --- Target Properties ---
# Output directory for the executable file (.exe)
set_target_properties(${SANDBOX_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${OUTPUT_DIR_BASE}/$<CONFIG>"
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