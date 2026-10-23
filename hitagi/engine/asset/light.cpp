export module asset:light;
import std;
import math;

export namespace hitagi::asset {
// Position/direction come from Transform; directional/spot lights point along -Z.
struct LightComponent {
    enum class Type : std::uint8_t {
        Point,
        Spot,
        Direction
    };
    Type        type             = Type::Spot;
    float       intensity        = 1.0f;
    math::Color color            = math::Color::White();
    float       inner_cone_angle = 30.0_deg;
    float       outer_cone_angle = 60.0_deg;
};
}  // namespace hitagi::asset
