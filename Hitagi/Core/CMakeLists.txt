add_subdirectory(HitagiMath)

add_library(MemoryManager   Buffer.cpp MemoryManager.cpp)
add_library(FileIOManager   FileIOManager.cpp)
add_library(Timer           Timer.cpp)
add_library(ThreadManager   ThreadManager.cpp)

target_link_libraries(MemoryManager PUBLIC Interface spdlog::spdlog HitagiMath)
target_link_libraries(FileIOManager PUBLIC Interface MemoryManager)
target_link_libraries(Timer         PUBLIC Interface)
target_link_libraries(ThreadManager PUBLIC Interface spdlog::spdlog PRIVATE $<$<PLATFORM_ID:Linux>:pthread>)


target_include_directories(MemoryManager INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(FileIOManager INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(Timer         INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(ThreadManager INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

