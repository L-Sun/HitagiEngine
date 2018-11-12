
add_executable(AssetLoaderTest AssetLoaderTest.cpp)
target_link_libraries(AssetLoaderTest Common)
add_test(NAME TEST_AssetLoader COMMAND AssetLoaderTest)

add_executable(OgexParserTest OgexParserTest.cpp)
target_link_libraries(OgexParserTest Common ${OPENGEX_LIBRARY} ${OPENDDL_LIBRARY} ${XG_LIBRARY})
add_test(NAME TEST_OgexParser COMMAND OgexParserTest)

# add_executable(SceneObjectTest SceneObjectTest.cpp)
# target_link_libraries(SceneObjectTest Common ${XG_LIBRARY})
# add_test(NAME TEST_SceneObject COMMAND SceneObjectTest)

add_executable(SceneLoadingTest SceneLoadingTest.cpp)
target_link_libraries(SceneLoadingTest Common ${OPENGEX_LIBRARY} ${OPENDDL_LIBRARY} ${XG_LIBRARY})
add_test(NAME TEST_SceneLoading COMMAND SceneLoadingTest)

add_executable(MathTest MathTest.cpp)
target_link_libraries(MathTest GeomMath ${GTEST_BOTH_LIBRARIES})
add_test(NAME TEST_Math COMMAND MathTest)