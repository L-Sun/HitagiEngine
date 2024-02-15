#pragma once
#include <hitagi/ecs/entity.hpp>
#include <hitagi/ecs/schedule.hpp>
#include <hitagi/math/transform.hpp>

#include <unordered_set>

namespace hitagi::asset {

struct RelationShip {
    RelationShip(ecs::Entity parent = {}) : parent(parent) {}

    ecs::Entity parent;

    const auto& GetChildren() const noexcept { return children; }

private:
    friend struct RelationShipSystem;
    ecs::Entity                          prev_parent = {};
    std::pmr::unordered_set<ecs::Entity> children;
};
static_assert(ecs::Component<RelationShip>);

struct RelationShipSystem {
    static void OnUpdate(ecs::Schedule& schedule);
};

struct Transform {
    Transform(math::vec3f position = math::vec3f(0.0f), math::quatf rotation = math::quatf::identity(), math::vec3f scaling = math::vec3f(1.0f))
        : position(position),
          rotation(rotation),
          scaling(scaling) {}

    math::vec3f position;
    math::quatf rotation;
    math::vec3f scaling;

    math::mat4f world_matrix = math::mat4f::identity();

    inline void ApplyScale(float value) noexcept { scaling = value; }
    inline void Translate(const math::vec3f& value) noexcept { position += value; }
    inline void Rotate(const math::quatf& value) noexcept { rotation = value * rotation; }

    inline auto ToMatrix() const noexcept { return math::translate(position) * math::rotate(rotation) * math::scale(scaling); }
};

struct TransformSystem {
    static void OnUpdate(ecs::Schedule& schedule);
};

}  // namespace hitagi::asset