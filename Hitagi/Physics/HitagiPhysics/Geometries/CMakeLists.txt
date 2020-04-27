add_library(Geometries Geometry.cpp Box.cpp Sphere.cpp Plane.cpp aabb.hpp)
target_link_libraries(Geometries PRIVATE HitagiMath)
target_include_directories(Geometries INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})