cmake_minimum_required(VERSION 3.15)
project(GameEngine)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDEBUG")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG")

find_package(OpenGL REQUIRED)
find_package(crossguid CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)



IF(${UNIX})
    set(TARGET_PLATFORM "Linux")
ELSEIF(${WIN32})
    set(TARGET_PLATFORM "Windows")
ENDIF(${UNIX})

include_directories("${PROJECT_SOURCE_DIR}/Platform/${TARGET_PLATFORM}")
set(EXTERNAL_ROOT ${PROJECT_SOURCE_DIR}/External/${TARGET_PLATFORM})
include_directories("${EXTERNAL_ROOT}/include")
include_directories("${EXTERNAL_ROOT}/include/bullet")
set(EXTERNAL_LIBRARY_PATH ${EXTERNAL_ROOT}/lib)


find_library(OPENDDL_LIBRARY OpenDDL PATHS ${EXTERNAL_LIBRARY_PATH} NO_CMAKE_FIND_ROOT_PATH NO_SYSTEM_ENVIRONMENT_PATH)
find_library(OPENGEX_LIBRARY OpenGEX PATHS ${EXTERNAL_LIBRARY_PATH} NO_CMAKE_FIND_ROOT_PATH NO_SYSTEM_ENVIRONMENT_PATH)
find_library(BULLET_COLLISION_LIBRARY NAMES BulletCollision PATHS ${EXTERNAL_LIBRARY_PATH} NO_CMAKE_FIND_ROOT_PATH NO_SYSTEM_ENVIRONMENT_PATH)
find_library(BULLET_DYNAMICS_LIBRARY NAMES BulletDynamics PATHS ${EXTERNAL_LIBRARY_PATH} NO_CMAKE_FIND_ROOT_PATH NO_SYSTEM_ENVIRONMENT_PATH)
find_library(BULLET_LINEARMATH_LIBRARY NAMES LinearMath PATHS ${EXTERNAL_LIBRARY_PATH} NO_CMAKE_FIND_ROOT_PATH NO_SYSTEM_ENVIRONMENT_PATH)
find_library(TURBO_JPEG_LIBRARY NAMES turbojpeg-static PATHS ${EXTERNAL_LIBRARY_PATH} NO_CMAKE_FIND_ROOT_PATH NO_SYSTEM_ENVIRONMENT_PATH)


include(CTest)
find_package(GTest  REQUIRED)
include(GoogleTest)
include_directories(${GTEST_INCLUDE_DIR})

add_subdirectory(Framework)
add_subdirectory(Game)
add_subdirectory(Physics)
add_subdirectory(Platform)
add_subdirectory(RHI)
add_subdirectory(Test)

enable_testing()

