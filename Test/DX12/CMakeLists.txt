add_executable(ContextTest ContextTest.cpp)
target_link_libraries(ContextTest
    PRIVATE
        D3DRHI
)

add_test(NAME TEST_D3DContextTest COMMAND ContextTest WORKING_DIRECTORY)
