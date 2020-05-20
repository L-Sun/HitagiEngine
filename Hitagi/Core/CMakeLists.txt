add_subdirectory(HitagiMath)

add_library(MemoryManager   Allocator.cpp Buffer.cpp MemoryManager.cpp)
add_library(FileIOManager   FileIOManager.cpp)
add_library(Timer           Timer.cpp)


target_link_libraries(MemoryManager PUBLIC Interface spdlog::spdlog)
target_link_libraries(FileIOManager PUBLIC Interface MemoryManager)
target_link_libraries(Timer         PUBLIC Interface)

target_include_directories(MemoryManager INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(FileIOManager INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(Timer INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

