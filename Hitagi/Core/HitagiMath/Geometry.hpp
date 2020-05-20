#pragma once
#include "Vector.hpp"

namespace Hitagi::Core {

struct Geometry {
};

struct Point : public Geometry {
    vec3f point;
};

struct Line : public Geometry {
    vec3f from, to;
};

struct Plane : public Geometry {
    vec3f position, normal;
};

struct Triangle : public Geometry {
    std::array<vec3f, 3> points;
};
struct Squrae : public Geometry {
    vec3f center;
    float length, width;
};

struct Sphere : public Geometry {
    vec3f position;
    float radius;
};

}  // namespace Hitagi::Core
