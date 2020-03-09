
add_library(HitagiEngineEmpty EmptyApplication.cpp EmptyApplicationConfig.cpp)
target_link_libraries(HitagiEngineEmpty
    PUBLIC
        BaseApplication
)