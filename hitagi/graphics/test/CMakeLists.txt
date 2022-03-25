add_executable(
    graphics_test
    graphics_test.cpp
)
target_link_libraries(
    graphics_test
    PRIVATE graphics
)
add_test(
    NAME TEST_graphics_test
    COMMAND graphics_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

if(WIN32)
    add_executable(
        dx12_test
        dx12_test.cpp
    )
    target_link_libraries(
        dx12_test
        PRIVATE dx12_driver_api
    )
    add_test(
        NAME TEST_dx12_test
        COMMAND dx12_test
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
endif(WIN32)
