
add_executable(BufferTest BufferTest.cpp)
target_link_libraries(BufferTest PRIVATE SceneManager)

add_executable(FileIOManagerTest FileIOManagerTest.cpp)
target_link_libraries(FileIOManagerTest PRIVATE FileIOManager)
add_test(NAME TEST_FileIOManager COMMAND FileIOManagerTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(SceneLoadingTest SceneLoadingTest.cpp)
target_link_libraries(SceneLoadingTest PRIVATE SceneManager)
add_test(NAME TEST_SceneLoading COMMAND SceneLoadingTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(ImageParserTest ImageParserTest.cpp)
target_link_libraries(ImageParserTest PRIVATE AssetManager GTest::gtest)
add_test(NAME TEST_ImageParser COMMAND ImageParserTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(MathTest MathTest.cpp)
target_link_libraries(MathTest PRIVATE HitagiMath GTest::gtest)
add_test(NAME TEST_Math COMMAND MathTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(TimerTest TimerTest.cpp)
target_link_libraries(TimerTest PRIVATE Timer GTest::gtest)
add_test(NAME TEST_TimerTest COMMAND TimerTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(TextBitmapTest TextBitmapTest.cpp)
target_link_libraries(TextBitmapTest PRIVATE freetype FileIOManager)

add_executable(ThreadPoolTest ThreadPoolTest.cpp)
target_link_libraries(ThreadPoolTest PRIVATE ThreadManager)
add_test(NAME TEST_ThreadPoolTest COMMAND ThreadPoolTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

add_executable(GraphicsTest GraphicsTest.cpp)
target_link_libraries(GraphicsTest PRIVATE GraphicsManager)
add_test(NAME TEST_GraphicsTest COMMAND GraphicsTest WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})

if(WIN32)
    add_executable(DX12Test DX12Test.cpp)
    target_link_libraries(DX12Test PRIVATE DX12DriverAPI)
    add_test(NAME TEST_DX12Test COMMAND DX12Test WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
endif(WIN32)
