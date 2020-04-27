
add_executable(BufferTest BufferTest.cpp)
target_link_libraries(BufferTest PRIVATE SceneManager)

add_executable(FileIOManagerTest FileIOManagerTest.cpp)
target_link_libraries(FileIOManagerTest PRIVATE Core)
add_test(NAME TEST_FileIOManager COMMAND FileIOManagerTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(SceneLoadingTest SceneLoadingTest.cpp)
target_link_libraries(SceneLoadingTest PRIVATE SceneManager)
add_test(NAME TEST_SceneLoading COMMAND SceneLoadingTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(ImageParserTest ImageParserTest.cpp)
target_link_libraries(ImageParserTest PRIVATE ResourceManager GTest::gtest)
add_test(NAME TEST_ImageParser COMMAND ImageParserTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(MathTest MathTest.cpp)
target_link_libraries(MathTest PRIVATE HitagiMath GTest::gtest)
add_test(NAME TEST_Math COMMAND MathTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(TimerTest TimerTest.cpp)
target_link_libraries(TimerTest PRIVATE Core GTest::gtest)
add_test(NAME TEST_TimerTest COMMAND TimerTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(TextBitmapTest TextBitmapTest.cpp)
target_link_libraries(TextBitmapTest PRIVATE freetype Core)

add_subdirectory(DX12)