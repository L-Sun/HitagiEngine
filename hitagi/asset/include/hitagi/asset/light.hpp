#pragma once
#include <hitagi/asset/resource.hpp>
#include <hitagi/math/transform.hpp>

namespace hitagi::asset {

class Light : public Resource {
    friend class LightNode;

public:
    enum struct Type : std::uint8_t {
        Point,
        Spot,
        Direction,
    };
    struct Parameters {
        Type        type             = Type::Spot;
        float       intensity        = 1.0f;
        math::vec3f color            = {1.0f, 1.0f, 1.0f};
        math::vec3f position         = {3.0f, 3.0f, 3.0f};
        math::vec3f direction        = {0.0f, -1.0f, 0.0f};
        math::vec3f up               = {0.0f, 1.0f, 0.0f};
        float       inner_cone_angle = 30.0_deg;
        float       outer_cone_angle = 60.0_deg;
    } parameters;

    Light(Parameters parameters, std::string_view name = "", xg::Guid guid = {})
        : Resource(name, guid), parameters(parameters) {}
};

}  // namespace hitagi::asset
