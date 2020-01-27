add_library(MyPhysics MyPhysicsManager.cpp)
target_link_libraries(MyPhysics
    PUBLIC 
        Geometries
        Common
)
target_include_directories(MyPhysics INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})