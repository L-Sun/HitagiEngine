add_executable(MyTest 
    MyTest.cpp
    MyTestConfig.cpp
)
target_link_libraries(MyTest
    PRIVATE
        HitagiEngine
        HitagiPhysics
)

