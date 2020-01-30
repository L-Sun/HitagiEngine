
add_executable(AssetLoaderTest AssetLoaderTest.cpp)
target_link_libraries(AssetLoaderTest PRIVATE AssetLoader)
add_test(NAME AssetLoader COMMAND AssetLoaderTest)

add_executable(OgexParserTest OgexParserTest.cpp)
target_link_libraries(OgexParserTest PRIVATE SceneManager)
add_test(NAME OgexParser COMMAND OgexParserTest)

# add_executable(SceneObjectTest SceneObjectTest.cpp)
# target_link_libraries(SceneObjectTest PRIVATE Common ${XG_LIBRARY})
# add_test(NAME SceneObject COMMAND SceneObjectTest)

add_executable(SceneLoadingTest SceneLoadingTest.cpp)
target_link_libraries(SceneLoadingTest PRIVATE SceneManager)
add_test(NAME SceneLoading COMMAND SceneLoadingTest)

add_executable(JpegParserTest JpegParserTest.cpp)
target_link_libraries(JpegParserTest PRIVATE AssetLoader Parser)
add_test(NAME JpegParser COMMAND JpegParserTest)

add_executable(PngParserTest PngParserTest.cpp)
target_link_libraries(PngParserTest PRIVATE AssetLoader Parser)
add_test(NAME PngParser COMMAND PngParserTest)


add_executable(MathTest MathTest.cpp)
target_link_libraries(MathTest PRIVATE GeomMath GTest::gtest)
add_test(NAME Math COMMAND MathTest)

add_executable(ColorSpaceConversionTest ColorSpaceConversionTest.cpp)
target_link_libraries(ColorSpaceConversionTest PRIVATE ColorSpaceConversion GTest::gtest)
add_test(NAME ColorSpaceConversion COMMAND ColorSpaceConversionTest)


add_executable(TimerTest TimerTest.cpp)
target_link_libraries(TimerTest PRIVATE Timer GTest::gtest)
add_test(NAME TimerTest COMMAND TimerTest)

add_executable(FBXTest FBXTest.cpp)
target_link_libraries(FBXTest PRIVATE ${FBX_LIBRARIES})
target_include_directories(FBXTest PRIVATE ${FBX_INCLUDE_DIR})
add_test(NAME FBX COMMAND FBXTest)
