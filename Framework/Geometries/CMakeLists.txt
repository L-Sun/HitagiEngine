add_library(Geometries Geometry.cpp Box.cpp Sphere.cpp Plane.cpp)
target_link_libraries(Geometries GeomMath)