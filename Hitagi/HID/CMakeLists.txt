add_library(InputManager InputManager.cpp)
target_link_libraries(InputManager PRIVATE Interface HitagiMath spdlog::spdlog)
target_include_directories(InputManager
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        "$<TARGET_PROPERTY:Application,INTERFACE_INCLUDE_DIRECTORIES>"
)