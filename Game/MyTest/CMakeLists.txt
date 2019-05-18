IF(ANDROID)
add_library(MyTest SHARED 
    MyTest.cpp MyTestConfig.cpp)
ELSEIF(APPLE)
add_executable(MyTest MACOSX_BUNDLE
    MyTest.cpp MyTestConfig.cpp)
ELSE(ANDROID)
add_executable(MyTest 
    MyTest.cpp MyTestConfig.cpp)
ENDIF(ANDROID)
target_link_libraries(MyTest 
                        MyGameEngineD3d12
                        MyPhysics
)
