
add_library(HitagiGameEngineEmpty EmptyApplication.cpp EmptyApplicationConfig.cpp)
target_link_libraries(HitagiGameEngineEmpty
    PUBLIC
        Common
)