cmake_minimum_required(VERSION 3.1)
project(GameEngine)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDEBUG")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/External/src/crossguid/cmake")

IF(${UNIX})
    set(TARGET_PLATFORM "Linux")
ELSEIF(${WIN32})
    set(TARGET_PLATFORM "Windows")
ENDIF(${UNIX})

include(TargetArchDetect)
target_architecture(TARGET_ARCH)
if((TARGET_ARCH MATCHES "x86_64" OR TARGET_ARCH MATCHES "ia64") AND NOT OF_32BIT)
        set(ARCH_BIT 64)
else()
        set(ARCH_BIT 32)
endif()

include_directories("${PROJECT_SOURCE_DIR}/Framework/Common")
include_directories("${PROJECT_SOURCE_DIR}/Framework/GeomMath")
include_directories("${PROJECT_SOURCE_DIR}/Framework/Geometries")
include_directories("${PROJECT_SOURCE_DIR}/Framework/Parser")
include_directories("${PROJECT_SOURCE_DIR}/Framework/Interface")
include_directories("${PROJECT_SOURCE_DIR}/Physics")
include_directories("${PROJECT_SOURCE_DIR}/RHI")

include_directories("${PROJECT_SOURCE_DIR}/External/src/glad/include")
include_directories("${PROJECT_SOURCE_DIR}/Platform/${TARGET_PLATFORM}")
set(EXTERNAL_ROOT ${PROJECT_SOURCE_DIR}/External/${TARGET_PLATFORM})
include_directories("${EXTERNAL_ROOT}/include")
include_directories("${EXTERNAL_ROOT}/include/bullet")
set(EXTERNAL_LIBRARY_PATH ${EXTERNAL_ROOT}/lib)

find_library(XG_LIBRARY xg PATHS ${EXTERNAL_LIBRARY_PATH} NO_CMAKE_FIND_ROOT_PATH NO_SYSTEM_ENVIRONMENT_PATH)
find_library(OPENDDL_LIBRARY OpenDDL PATHS ${EXTERNAL_LIBRARY_PATH} NO_CMAKE_FIND_ROOT_PATH NO_SYSTEM_ENVIRONMENT_PATH)
find_library(OPENGEX_LIBRARY OpenGEX PATHS ${EXTERNAL_LIBRARY_PATH} NO_CMAKE_FIND_ROOT_PATH NO_SYSTEM_ENVIRONMENT_PATH)
find_library(ZLIB_LIBRARY NAMES z zlib PATHS ${EXTERNAL_LIBRARY_PATH} NO_CMAKE_FIND_ROOT_PATH NO_SYSTEM_ENVIRONMENT_PATH)
find_library(BULLET_COLLISION_LIBRARY NAMES BulletCollision PATHS ${EXTERNAL_LIBRARY_PATH} NO_CMAKE_FIND_ROOT_PATH NO_SYSTEM_ENVIRONMENT_PATH)
find_library(BULLET_DYNAMICS_LIBRARY NAMES BulletDynamics PATHS ${EXTERNAL_LIBRARY_PATH} NO_CMAKE_FIND_ROOT_PATH NO_SYSTEM_ENVIRONMENT_PATH)
find_library(BULLET_LINEARMATH_LIBRARY NAMES LinearMath PATHS ${EXTERNAL_LIBRARY_PATH} NO_CMAKE_FIND_ROOT_PATH NO_SYSTEM_ENVIRONMENT_PATH)

include(PlatformDependencies)
include(CheckCXXSourceCompiles)
CHECK_CXX_SOURCE_COMPILES(
"#include <memory>
int main(int argc, char** argv)
{
    std::unique_ptr<int> my_int = std::make_unique<int>();
    *my_int = 0;
    return *my_int;
}
"
 HAVE_MAKE_UNIQUE)


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

