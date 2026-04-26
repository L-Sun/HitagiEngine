export module physics:components;
import std;
import math;
import ecs;

export namespace hitagi::physics {

inline constexpr std::uint32_t invalid_body_id = std::numeric_limits<std::uint32_t>::max();

enum struct ObjectLayer : std::uint8_t {
    NonMoving = 0,
    Moving    = 1,
};

enum struct MotionType : std::uint8_t {
    Static    = 0,
    Kinematic = 1,
    Dynamic   = 2,
};

constexpr auto default_object_layer(MotionType motion_type) noexcept -> ObjectLayer {
    return motion_type == MotionType::Static ? ObjectLayer::NonMoving : ObjectLayer::Moving;
}

struct PhysicsMaterial {
    float friction    = 0.2f;
    float restitution = 0.0f;
};
static_assert(ecs::Component<PhysicsMaterial>);

struct BoxCollider {
    math::vec3f half_extent{0.5f};
    math::vec3f offset{0.0f};
};
static_assert(ecs::Component<BoxCollider>);

struct SphereCollider {
    float       radius = 0.5f;
    math::vec3f offset{0.0f};
};
static_assert(ecs::Component<SphereCollider>);

using ColliderShape = std::variant<BoxCollider, SphereCollider>;

struct Collider {
    ColliderShape shape = BoxCollider{};

    Collider() = default;
    Collider(BoxCollider box) : shape(std::move(box)) {}
    Collider(SphereCollider sphere) : shape(std::move(sphere)) {}
};
static_assert(ecs::Component<Collider>);

struct RigidBody {
    MotionType      motion_type  = MotionType::Static;
    ObjectLayer     object_layer = default_object_layer(motion_type);
    PhysicsMaterial material{};

    math::vec3f linear_velocity{0.0f};
    math::vec3f angular_velocity{0.0f};

    float gravity_factor = 1.0f;
    bool  activate       = true;
    bool  allow_sleep    = true;
    bool  is_sensor      = false;

    std::uint32_t body_id = invalid_body_id;

    math::vec3f cached_position{0.0f};
    math::quatf cached_rotation = math::quatf::identity();
    math::vec3f cached_scaling{1.0f};
    bool        initialized = false;

    RigidBody() = default;
    explicit RigidBody(MotionType motion_type)
        : motion_type(motion_type), object_layer(default_object_layer(motion_type)) {}
    RigidBody(MotionType motion_type, ObjectLayer object_layer) : motion_type(motion_type), object_layer(object_layer) {}

    auto Valid() const noexcept -> bool { return body_id != invalid_body_id; }
};
static_assert(ecs::Component<RigidBody>);

}  // namespace hitagi::physics
