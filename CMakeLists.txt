cmake_minimum_required(VERSION 3.15)
project(HitagiEngine)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DDEBUG")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DDEBUG")

find_package(OpenGL REQUIRED)
find_package(crossguid CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(libpng CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(freetype CONFIG REQUIRED)

find_package(GTest CONFIG REQUIRED)
include(CTest)

add_subdirectory(Hitagi)
add_subdirectory(Test)
add_subdirectory(Examples)

enable_testing()

