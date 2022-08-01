#pragma once
#include <hitagi/math/vector.hpp>

#include <numbers>

namespace hitagi::resource {

struct Light {
    enum struct Type : std::uint8_t {
        Point,
        Spot,
        Direction,
    };

    Type        type             = Type::Point;
    float       intensity        = 1.0f;
    math::vec3f color            = math::vec3f(1.0f);
    float       outer_cone_angle = std::numbers::pi / 4.0f;
    float       inner_cone_angle = 0;  // default value is 0, means only outer cone angle is used

    math::vec3f position  = math::vec3f(0, 0, 1);
    math::vec3f direction = math::vec3f(0, 0, -1);
    math::vec3f up        = math::vec3f(0, 0, 1);
};

}  // namespace hitagi::resource
