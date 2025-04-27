# compass/CMakeLists.txt
set(EDITOR_NAME compass)

add_executable(${EDITOR_NAME})

target_sources(${EDITOR_NAME} PRIVATE
    src/main.cpp 
    src/EditorApplication.cpp
    src/UI/MainWindow.cpp
    # ... other editor sources
)

target_include_directories(${EDITOR_NAME} PRIVATE
    src 
)

# Link against the engine library
target_link_libraries(${EDITOR_NAME} PRIVATE
    wayfinder # Link to our engine
    # Add other editor-specific dependencies here (e.g., Dear ImGui)
    # imgui::imgui # Example
)

# --- Platform Specifics for the Executable ---
if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    message(STATUS "Editor: Configuring for Web (Emscripten)")
    set_target_properties(${EDITOR_NAME} PROPERTIES SUFFIX ".html")
    target_link_options(${EDITOR_NAME} PRIVATE
        "-s USE_GLFW=3"
        "-s ASSERTIONS=1"
        "-s WASM=1"
        "-s ASYNCIFY"
        "-s GL_ENABLE_GET_PROC_ADDRESS=1"
        # Add other Emscripten flags needed by the editor/game
    )
endif()

if (APPLE)
    message(STATUS "Editor: Linking Apple Frameworks")
    target_link_libraries(${EDITOR_NAME} PRIVATE
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
        # Add other frameworks needed by the editor/game
    )
endif()

message(STATUS "Editor: Configured target ${EDITOR_NAME}")
