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

target_link_libraries(MyTest
    PRIVATE
        $<IF:$<BOOL:${UseOpenGL}>,HitagiEngineOpenGL,HitagiEngineD3D12>
    HitagiPhysics
)

