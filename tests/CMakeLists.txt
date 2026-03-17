set(WAYFINDER_RENDER_TEST_NAME wayfinder_render_tests)

add_executable(${WAYFINDER_RENDER_TEST_NAME}
  rendering/RenderPipelineTests.cpp
)

wayfinder_set_output_directories(${WAYFINDER_RENDER_TEST_NAME})

target_link_libraries(${WAYFINDER_RENDER_TEST_NAME}
  PRIVATE
    ${ENGINE_NAME}
)

target_include_directories(${WAYFINDER_RENDER_TEST_NAME}
  PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/wayfinder/src
)

add_test(NAME ${WAYFINDER_RENDER_TEST_NAME} COMMAND ${WAYFINDER_RENDER_TEST_NAME})
set_target_properties(${WAYFINDER_RENDER_TEST_NAME} PROPERTIES FOLDER "${TESTS_DIRECTORY}")