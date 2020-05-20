add_library(HitagiPhysics HitagiPhysicsManager.cpp)
target_link_libraries(HitagiPhysics
    PUBLIC
        PhysicsManager
    PRIVATE
        spdlog::spdlog
)
target_include_directories(HitagiPhysics INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})