
add_library(MyGameEngineEmpty EmptyApplication.cpp EmptyApplicationConfig.cpp)
target_link_libraries(MyGameEngineEmpty
    PRIVATE
        Common
)