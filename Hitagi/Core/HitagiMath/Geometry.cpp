#include "./Geometry.hpp"

using namespace Hitagi;

Mesh Point::GenerateMesh() {
    return {
        .vertices       = {point},
        .indices        = {0},
        .primitive_type = PrimitiveType::PointList,
    };
}

Mesh Line::GenerateMesh() {
    return {
        .vertices       = {from, to},
        .indices        = {0, 1},
        .primitive_type = PrimitiveType::LineList,
    };
}

Mesh Plane::GenerateMesh() {
    return {};
}

Mesh Triangle::GenerateMesh() {
    return {
        .vertices       = std::vector<vec3f>(points.begin(), points.end()),
        .indices        = {0, 1, 2},
        .primitive_type = PrimitiveType::TriangleList,
    };
}

Mesh Squrae::GenerateMesh() {
    std::cerr << "Unimpletement function was invoked!" << std::endl;
    return {};
}

Mesh Box::GenerateMesh() {
    std::cerr << "Unimpletement function was invoked!" << std::endl;
    return {};
}

Mesh Circle::GenerateMesh() {
    std::cerr << "Unimpletement function was invoked!" << std::endl;
    return {};
}

Mesh Sphere::GenerateMesh() {
    std::cerr << "Unimpletement function was invoked!" << std::endl;
    return {};
}
