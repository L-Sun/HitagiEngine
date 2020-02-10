if(ANDROID)
    add_library(MyTest
        SHARED 
            MyTest.cpp
            MyTestConfig.cpp
    )
elseif(APPLE)
    add_executable(MyTest
        MACOSX_BUNDLE
            MyTest.cpp
            MyTestConfig.cpp
    )
else(ANDROID)
    add_executable(MyTest 
        MyTest.cpp
        MyTestConfig.cpp
    )
endif(ANDROID)

set(UseOpenGL false)

if(${UseOpenGL})
    target_link_libraries(MyTest 
        MyGameEngineOpenGL
        MyPhysics
    )
else()
    target_link_libraries(MyTest 
        MyGameEngineD3D12
        MyPhysics
    )
endif(${UseOpenGL})

