module;
#include "interop/tracy_macros.hpp"

export module physics:character;
import interop.tracy;
import std;
import math;
import ecs;
import asset;
import :types;
import :world;

namespace hitagi::physics {
using asset::AnimationPlayer;
using asset::RelationShip;
using asset::Transform;
}  // namespace hitagi::physics

export namespace hitagi::physics {
// The owner and joints are Scene entities with Transform/RelationShip.
// Skeleton topology stays fixed for this component's lifetime; reset rebuilds
// the physics world and binding caches after parameter changes.
struct CharacterPhysics {
    std::vector<ecs::Entity>  joints;
    std::vector<RigBody>      bodies;
    std::vector<RigJoint>     constraints;
    math::vec3f               gravity{0.0f, -98.0f, 0.0f};  // MMD decimetre-space convention
    bool                      enabled = true, reset = true;
    double                    last_time = -1.0, remainder = 0.0;
    double                    update_ms   = 0.0;
    float                     penetration = 0.0f;
    std::shared_ptr<RigWorld> world;
    std::vector<math::mat4f>  input_pose, previous_anchors;
    std::vector<int>          parent_indices, joint_bodies;
    std::vector<math::mat4f>  inverse_offsets;
};
struct CharacterPhysicsSystem {
    static void ConfigureSchedule(ecs::Schedule& schedule);
};
}  // namespace hitagi::physics

namespace hitagi::physics {
auto CharacterJointWorld(ecs::Entity entity) -> math::mat4f {
    auto pose = entity.Get<Transform>().ToMatrix();
    if (const auto parent = entity.Get<RelationShip>().parent)
        pose = CharacterJointWorld(parent) * pose;
    return pose;
}

auto InterpolateRigPose(const math::mat4f& a, const math::mat4f& b, float t) -> math::mat4f {
    const auto [ap, aq, as] = math::decompose(a);
    const auto [bp, bq, bs] = math::decompose(b);
    auto q                  = bq;
    if (math::dot(aq, q) < 0)
        for (auto& value : q.data) value = -value;
    float cosine = std::clamp(math::dot(aq, q), 0.0f, 1.0f);
    float wa = 1.0f - t, wb = t;
    if (cosine < 0.9995f) {
        const float angle = std::acos(cosine);
        wa                = std::sin((1 - t) * angle) / std::sin(angle);
        wb                = std::sin(t * angle) / std::sin(angle);
    }
    for (unsigned k = 0; k < 4; ++k) q[k] = aq[k] * wa + q[k] * wb;
    const float length = q.norm();
    for (auto& value : q.data) value /= length;
    return math::translate(ap * (1 - t) + bp * t) * math::rotate(q);
}

void CharacterPhysicsSystem::ConfigureSchedule(ecs::Schedule& schedule) {
    schedule.Request("restore_character_physics", [](ecs::Query<CharacterPhysics&> query) {
        query.Each([&](CharacterPhysics& rig) {
            // No saved pose on the first tick. Subsequent poses match the fixed skeleton.
            for (unsigned i = 0; i < rig.input_pose.size(); ++i) {
                auto& transform      = rig.joints[i].Get<Transform>();
                const auto [p, q, s] = math::decompose(rig.input_pose[i]);
                transform.position   = p;
                transform.rotation   = q;
                transform.scaling    = s;
            }
        });
    });
    schedule.Request("solve_character_physics", [&schedule](ecs::Query<ecs::Entity, CharacterPhysics&> query) {
        query.Each([&](ecs::Entity owner, CharacterPhysics& rig) {
            const auto begin = std::chrono::steady_clock::now();
            if (!rig.enabled) {
                rig.reset = true;
                return;
            }
            double delta = schedule.GetDeltaSeconds();
            if (owner.Has<AnimationPlayer>()) {
                const auto& player = owner.Get<AnimationPlayer>();
                double expected = rig.last_time + (player.playing ? delta * player.speed : 0.0);
                // Reaching the end of a non-looping clip is normal playback, not a seek.
                // Loop wraps still reset secondary motion at the discontinuity.
                if (player.clip && !player.looping)
                    expected = asset::ResolveAnimationTime(expected, player.clip->GetDuration(), false);
                if (rig.last_time >= 0.0 && std::abs(player.time - expected) > 1e-5)
                    rig.reset = true;
                rig.last_time = player.time;
                if (!player.playing) delta = 0;
            }
            if (!std::isfinite(delta) || delta < 0.0)
                throw std::invalid_argument("Invalid character physics delta");
            // A slow render frame must not recreate and warm up the physics world.
            // Explicit resets and animation discontinuities are handled separately.
            const auto owner_world   = CharacterJointWorld(owner);
            const auto owner_inverse = math::inverse(owner_world);
            rig.input_pose.clear();
            for (auto joint : rig.joints) rig.input_pose.emplace_back(joint.Get<Transform>().ToMatrix());
            if (rig.reset) {
                rig.parent_indices.assign(rig.joints.size(), -1);
                rig.joint_bodies.assign(rig.joints.size(), -1);
                rig.inverse_offsets.clear();
                for (unsigned i = 0; i < rig.joints.size(); ++i) {
                    const auto parent = rig.joints[i].Get<RelationShip>().parent;
                    const auto it     = std::ranges::find(rig.joints, parent);
                    if (it != rig.joints.end()) {
                        const int index = static_cast<int>(it - rig.joints.begin());
                        if (index >= static_cast<int>(i)) throw std::invalid_argument("Physics skeleton must be topologically ordered");
                        rig.parent_indices[i] = index;
                    }
                }
                for (unsigned i = 0; i < rig.bodies.size(); ++i) {
                    const auto& body = rig.bodies[i];
                    if (body.joint >= static_cast<int>(rig.joints.size()))
                        throw std::invalid_argument("Character body joint outside skeleton");
                    rig.inverse_offsets.emplace_back(math::inverse(body.bone_offset));
                    if (body.mode && body.joint >= 0) {
                        if (rig.joint_bodies[body.joint] >= 0)
                            throw std::invalid_argument("Multiple dynamic bodies drive the same joint");
                        rig.joint_bodies[body.joint] = static_cast<int>(i);
                    }
                }
            }
            // Evaluate the hierarchy once per pass, instead of walking every
            // ancestor again for every body and every physics substep.
            std::vector<math::mat4f> joint_world(rig.joints.size()), root_parent(rig.joints.size());
            for (unsigned i = 0; i < rig.joints.size(); ++i) {
                const int parent = rig.parent_indices[i];
                if (parent < 0) {
                    const auto entity = rig.joints[i].Get<RelationShip>().parent;
                    root_parent[i]    = entity ? owner_inverse * CharacterJointWorld(entity) : owner_inverse;
                }
                joint_world[i] = (parent < 0 ? root_parent[i] : joint_world[parent]) * rig.input_pose[i];
            }
            std::vector<math::mat4f> anchors;
            for (const auto& body : rig.bodies) {
                if (body.joint < 0)
                    anchors.emplace_back(body.bind);
                else
                    anchors.emplace_back(joint_world[body.joint] * body.bone_offset);
            }
            const auto apply = [&] {
                ZoneScopedNS("CharacterPhysics.ApplyPose", 0);
                // Skeleton order allows child local transforms to see the solved parent.
                for (unsigned j = 0; j < rig.joints.size(); ++j) {
                    const int   parent       = rig.parent_indices[j];
                    const auto& parent_world = parent < 0 ? root_parent[j] : joint_world[parent];
                    auto&       transform    = rig.joints[j].Get<Transform>();
                    const int   i            = rig.joint_bodies[j];
                    if (i < 0) {
                        joint_world[j] = parent_world * transform.ToMatrix();
                        continue;
                    }
                    const auto& body       = rig.bodies[i];
                    const auto  world_pose = rig.world->Pose(i) * rig.inverse_offsets[i];
                    const auto  local      = math::inverse(parent_world) * world_pose;
                    const auto [p, q, s]   = math::decompose(local);
                    transform.rotation     = q;
                    if (body.mode == 1) transform.position = p;
                    joint_world[j] = parent_world * transform.ToMatrix();
                    if (body.mode == 2) {
                        const auto        corrected = joint_world[j] * body.bone_offset;
                        const math::vec3f centre    = (corrected * math::vec4f(0, 0, 0, 1)).xyz;
                        rig.world->SetPosition(i, centre);
                    }
                }
            };
            constexpr float dt = 1.0f / 120.0f;
            if (rig.reset) {
                ZoneScopedNS("CharacterPhysics.ResetAndWarmup", 0);
                // Recreate to clear constraint warm starts and contact manifolds, too.
                rig.world = std::make_shared<RigWorld>(rig.bodies, rig.constraints, rig.gravity);
                rig.world->Reset(anchors);
                rig.previous_anchors = anchors;
                rig.remainder        = 0;
                for (unsigned i = 0; i < 120; ++i) {
                    rig.world->Step(anchors, dt);
                    apply();
                }
                rig.reset = false;
            }
            // Secondary motion gets at most eight 120 Hz steps per rendered frame.
            // Drop excess wall time instead of carrying an ever-growing backlog;
            // kinematic anchors still interpolate all the way to the current pose.
            constexpr unsigned max_steps = 8;
            rig.remainder += std::min(delta, max_steps / 120.0);
            const auto steps = std::min(max_steps, static_cast<unsigned>((rig.remainder + 1e-8) * 120.0));
            rig.remainder = std::max(0.0, rig.remainder - steps / 120.0);
            TracyPlot("CharacterPhysics.Substeps", static_cast<std::int64_t>(steps));
            std::vector<math::mat4f> interpolated(anchors.size());
            for (unsigned step = 0; step < steps; ++step) {
                ZoneScopedNS("CharacterPhysics.Substep", 0);
                const float t = static_cast<float>(step + 1) / steps;
                for (unsigned i = 0; i < anchors.size(); ++i)
                    if (rig.bodies[i].mode == 0)
                        interpolated[i] = InterpolateRigPose(rig.previous_anchors[i], anchors[i], t);
                rig.world->Step(interpolated, dt);
                apply();
            }
            if (!steps) apply();
            rig.previous_anchors = std::move(anchors);
            rig.penetration      = rig.world->MaxPenetration();
            rig.update_ms        = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
        });
    });
    schedule.SetOrder("restore_character_physics", "advance_animations");
    schedule.SetOrder("apply_animation_poses", "solve_character_physics");
    schedule.SetOrder("solve_character_physics", "update_local_matrix");
}
}  // namespace hitagi::physics
