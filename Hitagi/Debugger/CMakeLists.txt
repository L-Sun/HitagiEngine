add_library(DebugManager DebugManager.cpp)
target_link_libraries(DebugManager PUBLIC HitagiMath PRIVATE Interface spdlog::spdlog)
target_include_directories(DebugManager INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})