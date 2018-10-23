add_library(Common
Allocator.cpp
AssetLoader.cpp
BaseApplication.cpp
GraphicsManager.cpp
MemoryManager.cpp
SceneManager.cpp
main.cpp
)

find_library(XG_LIBRARY xg PATHS ${EXTERNAL_LIBRARY_PATH})
target_link_libraries(Common ${XG_LIBRARY})

if(WIN32)
    add_definitions(-DGUID_WINDOWS)
elseif(APPLE)
    find_library(CFLIB CoreFoundation)
    target_link_libraries(Common ${CFLIB})
    add_definitions(-DGUID_CFUUID)
    set(CMAKE_CXX_FLAGS "${CMAKE_CX_FLAGES} -Wall -pedantic")
elseif(ANDROID)
    target_compile_definitions(Common PRIVATE GUID_ANDROID)
else()
    find_package(Libuuid REQUIRED)
    if(NOT LIBUUID_FOUND)
        message(FATAL_ERROR "you need to install uuid")
    endif()
    include_directories(${LIBUUID_INCLUDE_DIR})
    target_link_libraries(Common ${LIBUUID_LIBRARY})
    add_definitions(-DGUID_LIBUUID)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -no-pie -pedantic")
endif(WIN32)


add_subdirectory(test)