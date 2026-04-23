module;

export module asset:transform;
import std;
import ecs;
import math;

export namespace hitagi::asset {

struct RelationShip {
    RelationShip(ecs::Entity parent = {}) : parent(parent) {}

    ecs::Entity parent;

    const auto& GetChildren() const noexcept { return children; }

private:
    friend struct RelationShipSystem;
    friend struct TransformSystem;
    ecs::Entity                          prev_parent = {};
    std::pmr::unordered_set<ecs::Entity> children;
    bool                                 subtree_dirty = true;
};
static_assert(ecs::Component<RelationShip>);

struct RelationShipSystem {
    static void OnUpdate(ecs::Schedule& schedule);
};

struct Transform {
    Transform(math::vec3f position = math::vec3f(0.0f), math::quatf rotation = math::quatf::identity(), math::vec3f scaling = math::vec3f(1.0f))
        : position(position),
          rotation(rotation),
          scaling(scaling),
          local_matrix(math::translate(position) * math::rotate(rotation) * math::scale(scaling)),
          world_matrix(local_matrix),
          cached_position(position),
          cached_rotation(rotation),
          cached_scaling(scaling) {}

    math::vec3f position;
    math::quatf rotation;
    math::vec3f scaling;

    math::mat4f local_matrix;
    math::mat4f world_matrix;

    inline void ApplyScale(float value) noexcept { scaling = value; }
    inline void Translate(const math::vec3f& value) noexcept { position += value; }
    inline void Rotate(const math::quatf& value) noexcept { rotation = value * rotation; }

    inline auto ToMatrix() const noexcept { return math::translate(position) * math::rotate(rotation) * math::scale(scaling); }

private:
    friend struct TransformSystem;
    math::vec3f cached_position;
    math::quatf cached_rotation;
    math::vec3f cached_scaling;
};

struct TransformSystem {
    static void OnUpdate(ecs::Schedule& schedule);
};

}  // namespace hitagi::asset
