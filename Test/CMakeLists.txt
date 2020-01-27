
add_executable(AssetLoaderTest AssetLoaderTest.cpp)
target_link_libraries(AssetLoaderTest AssetLoader)
add_test(NAME TEST_AssetLoader COMMAND AssetLoaderTest)

add_executable(OgexParserTest OgexParserTest.cpp)
target_link_libraries(OgexParserTest SceneManager)
add_test(NAME TEST_OgexParser COMMAND OgexParserTest)

# add_executable(SceneObjectTest SceneObjectTest.cpp)
# target_link_libraries(SceneObjectTest Common ${XG_LIBRARY})
# add_test(NAME TEST_SceneObject COMMAND SceneObjectTest)

add_executable(SceneLoadingTest SceneLoadingTest.cpp)
target_link_libraries(SceneLoadingTest SceneManager)
add_test(NAME TEST_SceneLoading COMMAND SceneLoadingTest)

add_executable(JpegParserTest JpegParserTest.cpp)
target_link_libraries(JpegParserTest AssetLoader Parser)
add_test(NAME TEST_JpegParser COMMAND JpegParserTest)

add_executable(PngParserTest PngParserTest.cpp)
target_link_libraries(PngParserTest AssetLoader Parser)
add_test(NAME TEST_PngParser COMMAND PngParserTest)


add_executable(MathTest MathTest.cpp)
target_link_libraries(MathTest GeomMath ${GTEST_BOTH_LIBRARIES})
gtest_add_tests(TARGET MathTest)

add_executable(ColorSpaceConversionTest ColorSpaceConversionTest.cpp)
target_link_libraries(ColorSpaceConversionTest ColorSpaceConversion ${GTEST_BOTH_LIBRARIES})
gtest_add_tests(TARGET ColorSpaceConversionTest)

add_executable(BulletTest BulletTest.cpp)
target_link_libraries(BulletTest ${BULLET_DYNAMICS_LIBRARY} ${BULLET_COLLISION_LIBRARY} ${BULLET_LINEARMATH_LIBRARY})
add_test(NAME TEST_Bullet COMMAND BulletTest)

add_executable(TimerTest TimerTest.cpp)
target_link_libraries(TimerTest Timer ${GTEST_BOTH_LIBRARIES})
gtest_add_tests(TARGET TimerTest)