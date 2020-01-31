
add_executable(AssetLoaderTest AssetLoaderTest.cpp)
target_link_libraries(AssetLoaderTest PRIVATE AssetLoader)
add_test(NAME TEST_AssetLoader COMMAND AssetLoaderTest)

add_executable(OgexParserTest OgexParserTest.cpp)
target_link_libraries(OgexParserTest PRIVATE SceneManager)
add_test(NAME TEST_OgexParser COMMAND OgexParserTest)

# add_executable(SceneObjectTest SceneObjectTest.cpp)
# target_link_libraries(SceneObjectTest PRIVATE Common ${XG_LIBRARY})
# add_test(NAME TEST_SceneObject COMMAND SceneObjectTest)

add_executable(SceneLoadingTest SceneLoadingTest.cpp)
target_link_libraries(SceneLoadingTest PRIVATE SceneManager)
add_test(NAME TEST_SceneLoading COMMAND SceneLoadingTest)

add_executable(JpegParserTest JpegParserTest.cpp)
target_link_libraries(JpegParserTest PRIVATE SceneManager)
add_test(NAME TEST_JpegParser COMMAND JpegParserTest)

add_executable(PngParserTest PngParserTest.cpp)
target_link_libraries(PngParserTest PRIVATE SceneManager)
add_test(NAME TEST_PngParser COMMAND PngParserTest)

add_executable(MathTest MathTest.cpp)
target_link_libraries(MathTest PRIVATE GeomMath GTest::gtest)
add_test(NAME TEST_Math COMMAND MathTest)

add_executable(TimerTest TimerTest.cpp)
target_link_libraries(TimerTest PRIVATE Timer GTest::gtest)
add_test(NAME TEST_TimerTest COMMAND TimerTest)

add_executable(FBXTest FBXTest.cpp)
target_link_libraries(FBXTest PRIVATE ${FBX_LIBRARIES} AssetLoader)
target_include_directories(FBXTest PRIVATE ${FBX_INCLUDE_DIR})
add_test(NAME TEST_FBX COMMAND FBXTest)
