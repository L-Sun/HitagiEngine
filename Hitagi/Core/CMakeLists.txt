add_library(Core            main.cpp)
add_library(GeomMath        STATIC IMPORTED GLOBAL)
add_library(MemoryManager   Allocator.cpp Buffer.cpp MemoryManager.cpp)
add_library(FileIOManager   FileIOManager.cpp)
add_library(Timer           Timer.cpp)

set(GEOMMATH_LIB_FILE ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_STATIC_LIBRARY_PREFIX}GeomMath${CMAKE_STATIC_LIBRARY_SUFFIX})
add_subdirectory(GeomMath)
add_dependencies(GeomMath ISPCMathLib)
target_include_directories(GeomMath INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(GeomMath
    PROPERTIES
        IMPORTED_LOCATION ${GEOMMATH_LIB_FILE})
target_link_libraries(GeomMath INTERFACE fmt::fmt)

target_link_libraries(MemoryManager PUBLIC Interface spdlog::spdlog)
target_link_libraries(FileIOManager PUBLIC Interface MemoryManager)
target_link_libraries(Timer         PUBLIC Interface)

target_include_directories(MemoryManager INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(FileIOManager INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(Timer INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(Core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(Core
    PUBLIC
        GeomMath
        MemoryManager
        FileIOManager
        Timer   
)