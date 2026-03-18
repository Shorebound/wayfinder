# WayfinderShaders.cmake — compile HLSL shaders to SPIR-V at build time using DXC
#
# Usage:
#   wayfinder_compile_shaders(TARGET <target> SHADERS <list of .vert/.frag files> OUTPUT_DIR <dir>)
#
# Expects DXC_EXECUTABLE to be set, or finds dxc.exe in tools/shadercompiler.

if(NOT DXC_EXECUTABLE)
    find_program(DXC_EXECUTABLE dxc
        HINTS "${CMAKE_SOURCE_DIR}/tools/shadercompiler/bin/x64"
    )
endif()

function(wayfinder_compile_shaders)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "" "TARGET;OUTPUT_DIR" "SHADERS")

    if(NOT DXC_EXECUTABLE)
        message(WARNING "DXC not found — skipping shader compilation. Set DXC_EXECUTABLE or install DXC.")
        return()
    endif()

    foreach(SHADER_SOURCE ${ARG_SHADERS})
        get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME)
        set(SPV_OUTPUT "${ARG_OUTPUT_DIR}/${SHADER_NAME}.spv")

        # Determine shader profile and binding shifts from extension
        get_filename_component(EXT ${SHADER_SOURCE} LAST_EXT)
        if(EXT STREQUAL ".vert")
            set(PROFILE "vs_6_0")
            set(ENTRY "VSMain")
        elseif(EXT STREQUAL ".frag")
            set(PROFILE "ps_6_0")
            set(ENTRY "PSMain")
        else()
            message(WARNING "Unknown shader extension: ${EXT} for ${SHADER_SOURCE}")
            continue()
        endif()

        add_custom_command(
            OUTPUT ${SPV_OUTPUT}
            COMMAND ${DXC_EXECUTABLE} -T ${PROFILE} -E ${ENTRY} -spirv
                    "${SHADER_SOURCE}" -Fo "${SPV_OUTPUT}"
            DEPENDS ${SHADER_SOURCE}
            COMMENT "Compiling shader ${SHADER_NAME} → ${SHADER_NAME}.spv"
            VERBATIM
        )
        list(APPEND SPV_OUTPUTS ${SPV_OUTPUT})
    endforeach()

    if(SPV_OUTPUTS)
        add_custom_target(${ARG_TARGET}_shaders ALL DEPENDS ${SPV_OUTPUTS})
        add_dependencies(${ARG_TARGET} ${ARG_TARGET}_shaders)
    endif()
endfunction()
