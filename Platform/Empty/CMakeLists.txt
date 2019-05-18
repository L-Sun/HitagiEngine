add_library(PlatformInterfaceEmpty 
                EmptyApplication.cpp
)
add_library(MyGameEngineEmpty EmptyApplicationConfig.cpp)
target_link_libraries(MyGameEngineEmpty
            Common 
            PlatformInterfaceEmpty
)