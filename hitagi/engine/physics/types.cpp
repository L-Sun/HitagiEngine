export module physics:types;
import std;
import math;

export namespace hitagi::physics {

enum struct ShapeType : std::uint8_t {
    Box,
    Sphere,
};

enum struct MotionType : std::uint8_t {
    Static,
    Kinematic,
    Dynamic,
};

enum struct Activation : std::uint8_t {
    DontActivate,
    Activate,
};

enum struct PhysicsUpdateError : std::uint32_t {
    None                   = 0,
    ManifoldCacheFull      = 1 << 0,
    BodyPairCacheFull      = 1 << 1,
    ContactConstraintsFull = 1 << 2,
};

constexpr auto operator|(PhysicsUpdateError lhs, PhysicsUpdateError rhs) noexcept -> PhysicsUpdateError {
    return static_cast<PhysicsUpdateError>(
        static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

constexpr auto operator&(PhysicsUpdateError lhs, PhysicsUpdateError rhs) noexcept -> PhysicsUpdateError {
    return static_cast<PhysicsUpdateError>(
        static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
}

constexpr auto has_error(PhysicsUpdateError errors, PhysicsUpdateError error) noexcept -> bool {
    return (errors & error) != PhysicsUpdateError::None;
}

struct BodyID {
    static constexpr std::uint32_t invalid_value = 0xffffffffu;

    std::uint32_t value = invalid_value;

    constexpr auto     IsValid() const noexcept -> bool { return value != invalid_value; }
    constexpr explicit operator bool() const noexcept { return IsValid(); }
    constexpr auto     operator==(const BodyID&) const noexcept -> bool = default;
};

struct BodyTransform {
    math::vec3f position = math::vec3f{0.0f, 0.0f, 0.0f};
    math::quatf rotation = math::quatf::identity();
};

struct ShapeDesc {
    ShapeType   type          = ShapeType::Box;
    math::vec3f half_extent   = math::vec3f{0.5f, 0.5f, 0.5f};
    float       radius        = 0.5f;
    float       convex_radius = 0.05f;

    static constexpr auto Box(math::vec3f half_extent, float convex_radius = 0.05f) noexcept -> ShapeDesc;
    static constexpr auto Sphere(float radius) noexcept -> ShapeDesc;
};

constexpr auto ShapeDesc::Box(math::vec3f half_extent, float convex_radius) noexcept -> ShapeDesc {
    ShapeDesc desc;
    desc.type          = ShapeType::Box;
    desc.half_extent   = half_extent;
    desc.convex_radius = convex_radius;
    return desc;
}

constexpr auto ShapeDesc::Sphere(float radius) noexcept -> ShapeDesc {
    ShapeDesc desc;
    desc.type   = ShapeType::Sphere;
    desc.radius = radius;
    return desc;
}

struct BodyDesc {
    ShapeDesc     shape;
    MotionType    motion_type = MotionType::Dynamic;
    BodyTransform transform;
    math::vec3f   linear_velocity  = math::vec3f{0.0f, 0.0f, 0.0f};
    math::vec3f   angular_velocity = math::vec3f{0.0f, 0.0f, 0.0f};
    float         friction         = 0.2f;
    float         restitution      = 0.0f;
    float         gravity_factor   = 1.0f;
    bool          allow_sleeping   = true;
    std::uint64_t user_data        = 0;
};

struct PhysicsWorldDesc {
    std::string   name                    = "PhysicsWorld";
    math::vec3f   gravity                 = math::vec3f{0.0f, -9.81f, 0.0f};
    std::uint32_t max_bodies              = 1024;
    std::uint32_t num_body_mutexes        = 0;
    std::uint32_t max_body_pairs          = 1024;
    std::uint32_t max_contact_constraints = 1024;
    std::size_t   temp_allocator_size     = 10 * 1024 * 1024;
};

// Authored rigid-body character rigs. Shapes and distances share one rig unit.
// The importer owns conversion from its source format; no parameters are guessed.
struct RigBody {
    int joint = -1;
    int mode = 0; // 0: animation, 1: full dynamics, 2: dynamic rotation
    int shape = 0; // sphere, box, Y capsule
    math::vec3f size{1.0f};
    math::mat4f bind = math::mat4f::identity();
    math::mat4f bone_offset = math::mat4f::identity();
    float mass = 1.0f, linear_damping = 0.0f, angular_damping = 0.0f;
    float friction = 0.5f, restitution = 0.0f;
    unsigned group = 1, mask = 65535;
};

struct RigJoint {
    unsigned a = 0, b = 0;
    math::mat4f bind = math::mat4f::identity();
    math::vec3f linear_min{0.0f}, linear_max{0.0f};
    math::vec3f angular_min{0.0f}, angular_max{0.0f};
    math::vec3f linear_spring{0.0f}, angular_spring{0.0f};
};
struct RigContact {
    float penetration = 0;
    int body_a = -1, body_b = -1;
};

enum class RigBackend { Bullet, Jolt };

}  // namespace hitagi::physics
