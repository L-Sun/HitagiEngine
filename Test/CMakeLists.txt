
add_executable(BufferTest BufferTest.cpp)
target_link_libraries(BufferTest PRIVATE SceneManager MemoryManager)

add_executable(AssetLoaderTest AssetLoaderTest.cpp)
target_link_libraries(AssetLoaderTest PRIVATE AssetLoader)
add_test(NAME TEST_AssetLoader COMMAND AssetLoaderTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(SceneLoadingTest SceneLoadingTest.cpp)
target_link_libraries(SceneLoadingTest PRIVATE SceneManager)
add_test(NAME TEST_SceneLoading COMMAND SceneLoadingTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(JpegParserTest JpegParserTest.cpp)
target_link_libraries(JpegParserTest PRIVATE SceneManager)
add_test(NAME TEST_JpegParser COMMAND JpegParserTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(PngParserTest PngParserTest.cpp)
target_link_libraries(PngParserTest PRIVATE SceneManager)
add_test(NAME TEST_PngParser COMMAND PngParserTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(MathTest MathTest.cpp)
target_link_libraries(MathTest PRIVATE GeomMath GTest::gtest)
add_test(NAME TEST_Math COMMAND MathTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(TimerTest TimerTest.cpp)
target_link_libraries(TimerTest PRIVATE Timer GTest::gtest)
add_test(NAME TEST_TimerTest COMMAND TimerTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
