#include "GeometryFactory.hpp"
#include "Types.hpp"

#include <Math/Vector.hpp>

using namespace Hitagi::Math;
using PrimitiveType = Hitagi::Graphics::PrimitiveType;

namespace Hitagi::Asset {
std::shared_ptr<Geometry> GeometryFactory::Line(const vec3f& from, const vec3f& to, std::shared_ptr<Material> material) {
    auto result = std::make_shared<Geometry>();
    Mesh mesh;

    mesh.AddVertexArray(VertexArray{
        "POSITION",
        VertexArray::DataType::Float3,
        // ! it is not guaranteed that sizeof(std::array<T, N>) == N * sizeof(T)
        Core::Buffer{(std::array{from, to}).data(), 2 * sizeof(vec3f)},
    });
    mesh.SetIndexArray(IndexArray{
        IndexArray::DataType::Int32,
        Core::Buffer{(std::array{0, 1}).data(), 2 * sizeof(unsigned)},
    });
    mesh.SetPrimitiveType(PrimitiveType::LineList);
    mesh.SetMaterial(material);

    result->AddMesh(mesh);
    return result;
}

std::shared_ptr<Geometry> GeometryFactory::Box(const vec3f& bb_min, const vec3f& bb_max, std::shared_ptr<Material> material, PrimitiveType primitive_type) {
    auto result = std::make_shared<Geometry>();
    Mesh mesh;

    std::array vertex_data = {
        bb_min,                               // 0
        vec3f(bb_max.x, bb_min.y, bb_min.z),  // 1
        vec3f(bb_min.x, bb_max.y, bb_min.z),  // 2
        vec3f(bb_min.x, bb_min.y, bb_max.z),  // 3
        bb_max,                               // 4
        vec3f(bb_min.x, bb_max.y, bb_max.z),  // 5
        vec3f(bb_max.x, bb_min.y, bb_max.z),  // 6
        vec3f(bb_max.x, bb_max.y, bb_min.z),  // 7
    };

    std::array index_data = {
        0u, 1u, 1u, 6u, 6u, 3u, 3u, 0u,  // front
        2u, 7u, 7u, 4u, 4u, 5u, 5u, 2u,  // back
        0u, 2u, 1u, 7u, 6u, 4u, 3u, 5u   // connect two plane
    };

    mesh.AddVertexArray(VertexArray{
        "POSITION",
        VertexArray::DataType::Float3,
        Core::Buffer{vertex_data.data(), vertex_data.size() * sizeof(vec3f)},
    });
    mesh.SetIndexArray(IndexArray{
        IndexArray::DataType::Int32,
        Core::Buffer{index_data.data(), index_data.size() * sizeof(unsigned)},
    });
    mesh.SetPrimitiveType(primitive_type);
    mesh.SetMaterial(material);

    result->AddMesh(mesh);
    return result;
}

}  // namespace Hitagi::Asset