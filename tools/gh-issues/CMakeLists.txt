set(GH_ISSUES_TARGET_NAME gh-issues)

add_executable(${GH_ISSUES_TARGET_NAME})
wayfinder_set_output_directories(${GH_ISSUES_TARGET_NAME})

target_sources(${GH_ISSUES_TARGET_NAME}
    PRIVATE
        src/Main.cpp
)

target_include_directories(${GH_ISSUES_TARGET_NAME}
    PRIVATE
        src
)

target_link_libraries(${GH_ISSUES_TARGET_NAME}
    PRIVATE
        nlohmann_json::nlohmann_json
        wayfinder_common
)
