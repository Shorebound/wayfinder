# waystone/CMakeLists.txt

# Define the executable target
add_executable(${SANDBOX_NAME})
message(STATUS "Sandbox: Configuring target ${SANDBOX_NAME}")

# --- Installation ---
#install(TARGETS ${SANDBOX_NAME}
#    RUNTIME DESTINATION bin 
#)

# --- Sources ---
target_sources(${SANDBOX_NAME} 
    PRIVATE
        src/WaystoneApplication.cpp 
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


# --- Custom Target to Copy Engine DLL ---
if(WIN32 AND WAYFINDER_BUILD_SHARED_LIBS)

    # Workaround: Manually construct the DLL path since standard generator expressions failed.
    set(MANUAL_DLL_SOURCE_PATH "$<TARGET_FILE_DIR:${ENGINE_NAME}>/${ENGINE_NAME}.dll")
    set(MANUAL_DESTINATION_DIR "$<TARGET_FILE_DIR:${SANDBOX_NAME}>")

    add_custom_target(CopyWayfinderDLL ALL # Add to default build
        # Command 1: Echo the manually constructed source path
        COMMAND ${CMAKE_COMMAND} -E echo "DEBUG (Manual Suffix): Source DLL = '${MANUAL_DLL_SOURCE_PATH}'"
        # Command 2: Echo the destination directory
        COMMAND ${CMAKE_COMMAND} -E echo "DEBUG (Manual Suffix): Destination Dir = '${MANUAL_DESTINATION_DIR}'"
        # Command 3: Perform the copy using the manually constructed paths
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MANUAL_DLL_SOURCE_PATH}"     # Use the variable holding the constructed source path
                "${MANUAL_DESTINATION_DIR}"      # Use the variable holding the destination path
        COMMENT "Copying ${ENGINE_NAME}.dll (Manual Suffix) to ${SANDBOX_NAME} directory"
        VERBATIM
    )

    # Dependencies remain the same
    add_dependencies(CopyWayfinderDLL ${ENGINE_NAME})
    add_dependencies(${SANDBOX_NAME} CopyWayfinderDLL)

    message(STATUS "Sandbox: Added custom target 'CopyWayfinderDLL' (Manual Suffix) to handle DLL copy.")

endif()


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
