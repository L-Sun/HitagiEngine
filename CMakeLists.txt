cmake_minimum_required(VERSION 3.15)
project(HitagiEngine)

set(CMAKE_CXX_STANDARD 20)

# set(CMAKE_CXX_CLANG_TIDY clang-tidy)

option(USE_ISPC "Use ispc to generate HitagiMath" ON)

find_package(crossguid CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(JPEG REQUIRED)
find_package(libpng CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(freetype CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(magic_enum CONFIG REQUIRED)

find_package(GTest CONFIG REQUIRED)
include(CTest)

add_subdirectory(Hitagi)
add_subdirectory(Test)
add_subdirectory(Examples)

enable_testing()