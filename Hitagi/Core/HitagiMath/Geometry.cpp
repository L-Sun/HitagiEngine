#include "./Geometry.hpp"

using namespace Hitagi;

std::pair<Geometry::Vertices, Geometry::Indices> Point::GenerateMesh() {
    return {{point}, {0}};
}

std::pair<Geometry::Vertices, Geometry::Indices> Line::GenerateMesh() {
    return {{from, to}, {0, 1, 0, 1, 1, 0}};
}

std::pair<Geometry::Vertices, Geometry::Indices> Plane::GenerateMesh() {
    std::cerr << "Unimpletement function was invoked!" << std::endl;
    return {{}, {}};
}

std::pair<Geometry::Vertices, Geometry::Indices> Triangle::GenerateMesh() {
    std::cerr << "Unimpletement function was invoked!" << std::endl;
    return {{}, {}};
}

std::pair<Geometry::Vertices, Geometry::Indices> Squrae::GenerateMesh() {
    std::cerr << "Unimpletement function was invoked!" << std::endl;
    return {{}, {}};
}

std::pair<Geometry::Vertices, Geometry::Indices> Box::GenerateMesh() {
    std::cerr << "Unimpletement function was invoked!" << std::endl;
    return {{}, {}};
}

std::pair<Geometry::Vertices, Geometry::Indices> Circle::GenerateMesh() {
    std::cerr << "Unimpletement function was invoked!" << std::endl;
    return {{}, {}};
}

std::pair<Geometry::Vertices, Geometry::Indices> Sphere::GenerateMesh() {
    std::cerr << "Unimpletement function was invoked!" << std::endl;
    return {{}, {}};
}
