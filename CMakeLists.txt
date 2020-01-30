cmake_minimum_required(VERSION 3.15)
project(GameEngine)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDEBUG")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(OpenGL REQUIRED)
find_package(crossguid CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(libpng CONFIG REQUIRED)
find_package(FBX REQUIRED)

IF(${UNIX})
    set(TARGET_PLATFORM "Linux")
ELSEIF(${WIN32})
    set(TARGET_PLATFORM "Windows")
ENDIF(${UNIX})

include_directories("${PROJECT_SOURCE_DIR}/Platform/${TARGET_PLATFORM}")
include_directories("${PROJECT_SOURCE_DIR}/External/include")
set(EXTERNAL_ROOT ${PROJECT_SOURCE_DIR}/External/${TARGET_PLATFORM})
set(EXTERNAL_LIBRARY_PATH ${EXTERNAL_ROOT}/lib)


find_library(OPENDDL_LIBRARY OpenDDL PATHS ${EXTERNAL_LIBRARY_PATH} NO_CMAKE_FIND_ROOT_PATH NO_SYSTEM_ENVIRONMENT_PATH)
find_library(OPENGEX_LIBRARY OpenGEX PATHS ${EXTERNAL_LIBRARY_PATH} NO_CMAKE_FIND_ROOT_PATH NO_SYSTEM_ENVIRONMENT_PATH)

find_package(GTest CONFIG REQUIRED)
include(CTest)

add_subdirectory(Framework)
add_subdirectory(Game)
add_subdirectory(Physics)
add_subdirectory(Platform)
add_subdirectory(RHI)
add_subdirectory(Test)

enable_testing()

