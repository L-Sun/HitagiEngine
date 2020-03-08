if(ANDROID)
    add_library(HitagiTest
        SHARED 
            HitagiTest.cpp
            HitagiTestConfig.cpp
    )
elseif(APPLE)
    add_executable(HitagiTest
        MACOSX_BUNDLE
            HitagiTest.cpp
            HitagiTestConfig.cpp
    )
else(ANDROID)
    add_executable(HitagiTest 
        HitagiTest.cpp
        HitagiTestConfig.cpp
    )
endif(ANDROID)

set(UseOpenGL true)

if(${UseOpenGL})
    target_link_libraries(HitagiTest 
        HitagiGameEngineOpenGL
        HitagiPhysics
    )
else()
    target_link_libraries(HitagiTest 
        HitagiGameEngineD3D12
        HitagiPhysics
    )
endif(${UseOpenGL})

