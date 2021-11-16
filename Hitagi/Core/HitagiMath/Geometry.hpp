#pragma once
#include "Vector.hpp"

#include <vector>

namespace Hitagi {
class Geometry {
public:
    using Vertices                                      = std::vector<vec3f>;
    using Indices                                       = std::vector<uint32_t>;
    virtual ~Geometry()                                 = default;
    virtual std::pair<Vertices, Indices> GenerateMesh() = 0;
};

struct Point : public Geometry {
    Point(const vec3f& point) : point(point) {}
    std::pair<Vertices, Indices> GenerateMesh() final;

    vec3f point;
};

struct Line : public Geometry {
    Line(const vec3f& from, const vec3f& to) : from(from), to(to) {}
    std::pair<Vertices, Indices> GenerateMesh() final;

    vec3f from, to;
};

struct Plane : public Geometry {
    Plane(const vec3f& position, const vec3f& normal) : position(position), normal(normal) {}
    std::pair<Vertices, Indices> GenerateMesh() final;

    vec3f position, normal;
};

struct Triangle : public Geometry {
    Triangle(const vec3f& p1, const vec3f& p2, const vec3f& p3) : points{p1, p2, p3} {}
    std::pair<Vertices, Indices> GenerateMesh() final;

    std::array<vec3f, 3> points;
};

struct Squrae : public Geometry {
    Squrae(const vec3f& center, float length, float width) : center(center), length(length), width(width) {}
    std::pair<Vertices, Indices> GenerateMesh() final;

    vec3f center;
    float length, width;
};

struct Box : public Geometry {
    Box(const vec3f& bb_min, const vec3f& bb_max) : bb_min(bb_min), bb_max(bb_max) {}
    std::pair<Vertices, Indices> GenerateMesh() final;

    vec3f bb_min, bb_max;
};

struct Circle : public Geometry {
    Circle(const vec3f& position, const vec3f& normal, float radius) : position(position), normal(normal), radius(radius) {}
    std::pair<Vertices, Indices> GenerateMesh() final;

    vec3f position, normal;
    float radius;
};

struct Sphere : public Geometry {
    Sphere(const vec3f& position, float radius) : position(position), radius(radius) {}
    std::pair<Vertices, Indices> GenerateMesh() final;

    vec3f position;
    float radius;
};

}  // namespace Hitagi
