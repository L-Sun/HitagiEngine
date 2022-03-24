#pragma once
#include "Geometry.hpp"
#include "Types.hpp"

#include <Math/Vector.hpp>

namespace Hitagi::Asset {
class GeometryFactory {
public:
    static std::shared_ptr<Geometry> Line(const Math::vec3f& from, const Math::vec3f& to, std::shared_ptr<Material> material = nullptr);
    static std::shared_ptr<Geometry> Box(const Math::vec3f& bb_min, const Math::vec3f& bb_max, std::shared_ptr<Material> material = nullptr, Graphics::PrimitiveType primitive_type = Graphics::PrimitiveType::LineList);
};
}  // namespace Hitagi::Asset