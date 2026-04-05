# tools/shader_manifest — build-time tool that generates shader_manifest.json
#
# Compiles each .slang program via the Slang API for both vertex and fragment
# entry points, extracts per-stage resource counts via Slang reflection, and
# writes a single JSON manifest consumed by ShaderManager in Shipping builds.

set(SHADER_MANIFEST_NAME wayfinder_shader_manifest)

add_executable(${SHADER_MANIFEST_NAME}
    main.cpp
)

wayfinder_set_output_directories(${SHADER_MANIFEST_NAME})
set_target_properties(${SHADER_MANIFEST_NAME} PROPERTIES FOLDER "${TOOLS_DIRECTORY}")

target_link_libraries(${SHADER_MANIFEST_NAME}
    PRIVATE
        ${ENGINE_NAME}
        nlohmann_json::nlohmann_json
)

# Copy Slang runtime DLLs next to the tool executable so it can run at build time
if(WIN32 AND DEFINED SLANG_BIN_DIR)
    set(_SLANG_MANIFEST_DLLS
        "${SLANG_BIN_DIR}/slang.dll"
        "${SLANG_BIN_DIR}/slang-rt.dll"
        "${SLANG_BIN_DIR}/slang-compiler.dll"
        "${SLANG_BIN_DIR}/slang-glslang.dll"
    )
    foreach(_DLL ${_SLANG_MANIFEST_DLLS})
        if(EXISTS "${_DLL}")
            add_custom_command(TARGET ${SHADER_MANIFEST_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_DLL}" "$<TARGET_FILE_DIR:${SHADER_MANIFEST_NAME}>"
                VERBATIM
            )
        endif()
    endforeach()
endif()
