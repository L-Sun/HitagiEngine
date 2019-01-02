
add_executable(AssetLoaderTest AssetLoaderTest.cpp)
target_link_libraries(AssetLoaderTest Common ${ZLIB_LIBRARY})
add_test(NAME TEST_AssetLoader COMMAND AssetLoaderTest)

add_executable(OgexParserTest OgexParserTest.cpp)
target_link_libraries(OgexParserTest Common ${OPENGEX_LIBRARY} ${OPENDDL_LIBRARY} ${XG_LIBRARY} ${ZLIB_LIBRARY})
__add_xg_platform_dependencies(OgexParserTest)
add_test(NAME TEST_OgexParser COMMAND OgexParserTest)

# add_executable(SceneObjectTest SceneObjectTest.cpp)
# target_link_libraries(SceneObjectTest Common ${XG_LIBRARY})
# __add_xg_platform_dependencies(SceneObjectTest)
# add_test(NAME TEST_SceneObject COMMAND SceneObjectTest)

add_executable(SceneLoadingTest SceneLoadingTest.cpp)
target_link_libraries(SceneLoadingTest Common ${OPENGEX_LIBRARY} ${OPENDDL_LIBRARY} ${XG_LIBRARY} ${ZLIB_LIBRARY})
add_test(NAME TEST_SceneLoading COMMAND SceneLoadingTest)

add_executable(JpegParserTest JpegParserTest.cpp)
target_link_libraries(JpegParserTest Common ${OPENGEX_LIBRARY} ${OPENDDL_LIBRARY} ${XG_LIBRARY} ${ZLIB_LIBRARY})
add_test(NAME TEST_JpegParser COMMAND JpegParserTest)

add_executable(PngParserTest PngParserTest.cpp)
target_link_libraries(PngParserTest Common ${OPENGEX_LIBRARY} ${OPENDDL_LIBRARY} ${XG_LIBRARY} ${ZLIB_LIBRARY})
add_test(NAME TEST_PngParser COMMAND PngParserTest)


add_executable(MathTest MathTest.cpp)
target_link_libraries(MathTest GeomMath ${GTEST_BOTH_LIBRARIES})
gtest_add_tests(TARGET MathTest)

add_executable(ColorSpaceConversionTest ColorSpaceConversionTest.cpp)
target_link_libraries(ColorSpaceConversionTest GeomMath ${GTEST_BOTH_LIBRARIES})
gtest_add_tests(TARGET ColorSpaceConversionTest)