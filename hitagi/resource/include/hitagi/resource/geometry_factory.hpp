#pragma once
#include "geometry.hpp"

#include <hitagi/math/vector.hpp>

namespace hitagi::asset {
class GeometryFactory {
public:
    static std::shared_ptr<Geometry> Line(const math::vec3f& from, const math::vec3f& to, std::shared_ptr<Material> material = nullptr);
    static std::shared_ptr<Geometry> Box(const math::vec3f& bb_min, const math::vec3f& bb_max, std::shared_ptr<Material> material = nullptr, PrimitiveType primitive_type = graphics::PrimitiveType::LineList);
};
}  // namespace hitagi::asset