#pragma once
#include "Geometry.hpp"

namespace Hitagi::Asset {
class GeometryFactory {
public:
    static std::shared_ptr<Geometry> Line(const vec3f& from, const vec3f& to, std::shared_ptr<Material> material = nullptr);
    static std::shared_ptr<Geometry> Box(const vec3f& bb_min, const vec3f& bb_max, std::shared_ptr<Material> material = nullptr, PrimitiveType primitive_type = PrimitiveType::LineList);
};
}  // namespace Hitagi::Asset