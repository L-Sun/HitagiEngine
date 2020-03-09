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

# set(UseOpenGL true)

if(${UseOpenGL})
    target_link_libraries(MyTest 
        HitagiEngineOpenGL
        HitagiPhysics
    )
else()
    target_link_libraries(MyTest 
        HitagiEngineD3D12
        HitagiPhysics
    )
endif(${UseOpenGL})

