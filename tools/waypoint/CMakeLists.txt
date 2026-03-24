add_executable(${ASSET_PROCESSOR_NAME})
wayfinder_set_output_directories(${ASSET_PROCESSOR_NAME})
message(STATUS "Tools: Configuring target ${ASSET_PROCESSOR_NAME}")

target_sources(${ASSET_PROCESSOR_NAME}
    PRIVATE
        src/MeshImporter.cpp
        src/MeshImporter.h
        src/WaypointMain.cpp
)

target_include_directories(${ASSET_PROCESSOR_NAME}
    PRIVATE
        src
)

target_link_libraries(${ASSET_PROCESSOR_NAME}
    PRIVATE
        ${ENGINE_NAME}
        fastgltf::fastgltf
        meshoptimizer
)

