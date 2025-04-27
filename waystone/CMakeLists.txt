# waystone/CMakeLists.txt

# Use the variable defined in the root CMakeLists for consistency
# set(APP_NAME waystone) # Or keep explicit name if preferred
set(APP_NAME ${SANDBOX_NAME})

# Define the executable target
add_executable(${APP_NAME})
message(STATUS "Sandbox: Configuring target ${APP_NAME}")

# --- Sources ---
# List source files for the sandbox executable
target_sources(${APP_NAME} PRIVATE
    src/main.cpp             # The main entry point MUST be here!
    # src/WaystoneApplication.cpp # Add your sandbox app class here later
    # ... other sandbox-specific .cpp files
)

# --- Include Directories ---
target_include_directories(${APP_NAME} PRIVATE
    # PRIVATE: Only needed for includes within waystone's own source files
    src
    # NOTE: We DO NOT need to add wayfinder's include path here.
    # target_link_libraries(wayfinder) automatically propagates the PUBLIC
    # include directories from wayfinder (${CMAKE_CURRENT_SOURCE_DIR}/src
    # and the generated export header path).
)

# --- Link Libraries ---
target_link_libraries(${APP_NAME} PRIVATE
    # PRIVATE: Link against the engine library. Waystone uses wayfinder,
    # but doesn't expose wayfinder's API itself.
    ${ENGINE_NAME} # Use the variable for the engine name
    # Add any other libraries needed ONLY by this specific sandbox executable
)

# --- Platform Specifics for THIS Executable ---
# These settings affect the final linking stage of the waystone executable

# Web Configurations (using standard CMake variable)
if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    message(STATUS "Sandbox: Configuring ${APP_NAME} for Web (Emscripten)")
    set_target_properties(${APP_NAME} PROPERTIES SUFFIX ".html")
    # Use target_link_options (CMake >= 3.13) for cleaner linker flags
    target_link_options(${APP_NAME} PRIVATE
        "-s USE_GLFW=3"
        "-s ASSERTIONS=1"
        "-s WASM=1"
        "-s ASYNCIFY"
        "-s GL_ENABLE_GET_PROC_ADDRESS=1"
        # Add other Emscripten flags needed by the sandbox/game
    )
    # Old way (still works):
    # set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s USE_GLFW=3 ...")
endif()

# Apple Framework Linking
if (APPLE)
    message(STATUS "Sandbox: Linking Apple Frameworks for ${APP_NAME}")
    target_link_libraries(${APP_NAME} PRIVATE
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
        # Add other frameworks needed by the sandbox/game
    )
endif()
