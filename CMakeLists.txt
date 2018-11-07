cmake_minimum_required(VERSION 3.1)
project(GameEngine)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-Wall")
set(CMAKE_CXX_FLAGS "-g -no-pie")
set(CMAKE_CXX_FLAGS_DEBUG "-D_DEBUG")

include(CTest)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/Platform")
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/External/src/crossguid/cmake")

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

# include(PlatformDependencies)
add_subdirectory(Framework)
add_subdirectory(Platform)
add_subdirectory(RHI)
add_subdirectory(Test)