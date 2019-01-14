add_library(Common
Allocator.cpp
AssetLoader.cpp
BaseApplication.cpp
GraphicsManager.cpp
MemoryManager.cpp
Scene.cpp
SceneObject.cpp
SceneManager.cpp
InputManager.cpp
PhysicsManager.cpp
Image.cpp
main.cpp
)

target_link_libraries(Common 
        GeomMath
        ${OPENGEX_LIBRARY} 
        ${OPENDDL_LIBRARY} 
        ${XG_LIBRARY} 
        ${ZLIB_LIBRARY}
        ${BULLET_DYNAMICS_LIBRARY} 
        ${BULLET_COLLISION_LIBRARY} 
        ${BULLET_LINEARMATH_LIBRARY})

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
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic")
endif()

