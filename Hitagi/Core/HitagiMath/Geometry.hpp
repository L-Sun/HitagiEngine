#pragma once
#include "Vector.hpp"

namespace Hitagi {
class Geometry {
public:
    virtual ~Geometry(){};

private:
};

struct Point : public Geometry {
    Point(const vec3f& point) : point(point) {}
    vec3f point;
};

struct Line : public Geometry {
    Line(const vec3f& from, const vec3f& to) : from(from), to(to) {}
    vec3f from, to;
};

struct Plane : public Geometry {
    Plane(const vec3f& position, const vec3f& normal) : position(position), normal(normal) {}
    vec3f position, normal;
};

struct Triangle : public Geometry {
    Triangle(const vec3f& p1, const vec3f& p2, const vec3f& p3) : points{p1, p2, p3} {}
    std::array<vec3f, 3> points;
};

struct Squrae : public Geometry {
    Squrae(const vec3f& center, float length, float width) : center(center), length(length), width(width) {}
    vec3f center;
    float length, width;
};

struct Box : public Geometry {
    Box(const vec3f& bbMin, const vec3f& bbMax) : bbMin(bbMin), bbMax(bbMax) {}
    vec3f bbMin, bbMax;
};

struct Circle : public Geometry {
    Circle(const vec3f& position, const vec3f& normal, float radius) : position(position), normal(normal), radius(radius) {}
    vec3f position, normal;
    float radius;
};

struct Sphere : public Geometry {
    Sphere(const vec3f& position, float radius) : position(position), radius(radius) {}
    vec3f position;
    float radius;
};

}  // namespace Hitagi
