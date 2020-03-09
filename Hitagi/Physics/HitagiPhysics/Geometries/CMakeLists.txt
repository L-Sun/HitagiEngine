add_library(Geometries Geometry.cpp Box.cpp Sphere.cpp Plane.cpp)
target_link_libraries(Geometries PRIVATE GeomMath)
target_include_directories(Geometries INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})