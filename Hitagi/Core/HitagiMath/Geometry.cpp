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
    return {
        .vertices = {
            bb_min,                                // 0
            vec3f(bb_max.x, bb_min.y, bb_min.z),   // 1
            vec3f(bb_min.x, bb_max.y, bb_min.z),   // 2
            vec3f(bb_min.x, bb_min.y, bb_max.z),   // 3
            bb_max,                                // 4
            vec3f(bb_min.x, bb_max.y, bb_max.z),   // 5
            vec3f(bb_max.x, bb_min.y, bb_max.z),   // 6
            vec3f(bb_max.x, bb_max.y, bb_min.z)},  // 7
        .indices = {
            0, 1, 1, 6, 6, 3, 3, 0,  // front
            2, 7, 7, 4, 4, 5, 5, 2,  // back
            0, 2, 1, 7, 6, 4, 3, 5   // connect two plane
        },
        .primitive_type = PrimitiveType::LineList,
    };
}

Mesh Circle::GenerateMesh() {
    std::cerr << "Unimpletement function was invoked!" << std::endl;
    return {};
}

Mesh Sphere::GenerateMesh() {
    std::cerr << "Unimpletement function was invoked!" << std::endl;
    return {};
}
