cmake_minimum_required(VERSION 3.1)
project(GameEngine)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_FLAGS_DEBUG "-D_DEBUG")


list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/Platform")
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/External/src/crossguid/cmake")

include(TargetArchDetect)
target_architecture(TARGET_ARCH)
if((TARGET_ARCH MATCHES "x86_64" OR TARGET_ARCH MATCHES "ia64") AND NOT OF_32BIT)
        set(ARCH_BIT 64)
else()
        set(ARCH_BIT 32)
endif()

include_directories("${PROJECT_SOURCE_DIR}/Framework/Common")
include_directories("${PROJECT_SOURCE_DIR}/Framework/GeomMath")
include_directories("${PROJECT_SOURCE_DIR}/Framework/Parser")
include_directories("${PROJECT_SOURCE_DIR}/Framework/Interface")
include_directories("${PROJECT_SOURCE_DIR}/RHI")

include_directories("${PROJECT_SOURCE_DIR}/External/src/glm/glm")
include_directories("${PROJECT_SOURCE_DIR}/External/src/glad/include")

IF(${UNIX})
    set(TARGET_PLATFORM "Linux")
ELSEIF(${WIN32})
    set(TARGET_PLATFORM "Windows")
ENDIF(${UNIX})
include_directories("${PROJECT_SOURCE_DIR}/Platform/${TARGET_PLATFORM}")
set(EXTERNAL_ROOT ${PROJECT_SOURCE_DIR}/External/${TARGET_PLATFORM})
include_directories("${EXTERNAL_ROOT}/include")
set(EXTERNAL_LIBRARY_PATH ${EXTERNAL_ROOT}/lib)

include(PlatformDependencies)
include(CheckCXXSourceCompiles)
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

include(CTest)
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_subdirectory(Framework)
add_subdirectory(Platform)
add_subdirectory(RHI)
add_subdirectory(Test)

enable_testing()