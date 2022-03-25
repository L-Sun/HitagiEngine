add_library(utils INTERFACE)
target_include_directories(utils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(test_utils INTERFACE)
target_link_libraries(test_utils INTERFACE GTest::gtest GTest::gmock)
target_include_directories(
    test_utils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
