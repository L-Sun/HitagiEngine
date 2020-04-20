
add_executable(BufferTest BufferTest.cpp)
target_link_libraries(BufferTest PRIVATE SceneManager)

add_executable(FileIOManagerTest FileIOManagerTest.cpp)
target_link_libraries(FileIOManagerTest PRIVATE Core)
add_test(NAME TEST_FileIOManager COMMAND FileIOManagerTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(SceneLoadingTest SceneLoadingTest.cpp)
target_link_libraries(SceneLoadingTest PRIVATE SceneManager)
add_test(NAME TEST_SceneLoading COMMAND SceneLoadingTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(JpegParserTest JpegParserTest.cpp)
target_link_libraries(JpegParserTest PRIVATE SceneManager)
add_test(NAME TEST_JpegParser COMMAND JpegParserTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(TgaParserTest TgaParserTest.cpp)
target_link_libraries(TgaParserTest PRIVATE SceneManager)
add_test(NAME TEST_TgaParser COMMAND TgaParserTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(PngParserTest PngParserTest.cpp)
target_link_libraries(PngParserTest PRIVATE SceneManager)
add_test(NAME TEST_PngParser COMMAND PngParserTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(MathTest MathTest.cpp)
target_link_libraries(MathTest PRIVATE GeomMath GTest::gtest)
add_test(NAME TEST_Math COMMAND MathTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(TimerTest TimerTest.cpp)
target_link_libraries(TimerTest PRIVATE Core GTest::gtest)
add_test(NAME TEST_TimerTest COMMAND TimerTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(TextBitmapTest TextBitmapTest.cpp)
target_link_libraries(TextBitmapTest PRIVATE freetype Core)

add_subdirectory(DX12)