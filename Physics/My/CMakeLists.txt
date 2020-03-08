add_library(HitagiPhysics HitagiPhysicsManager.cpp)
target_link_libraries(HitagiPhysics
    PUBLIC 
        Geometries
        Common
)
target_include_directories(HitagiPhysics INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})