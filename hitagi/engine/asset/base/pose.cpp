export module asset.base:pose;
import std;
import math;
import ecs;
import :animation;
import :transform;

export namespace hitagi::asset {

using Pose = std::pmr::vector<LocalPose>;

struct JointSlot {
    std::uint32_t target = 0;
    std::int32_t  parent = -1;
    LocalPose     rest;
};

// A validated computation layout, not a second editable scene hierarchy.
// Target IDs are stable within this rig. Every intermediate transform must be
// included; parents precede children. Rest poses are explicit authoring inputs.
class JointLayout {
public:
    explicit JointLayout(std::span<const JointSlot> joints);
    auto GetJoints() const noexcept -> std::span<const JointSlot> { return m_Joints; }
    auto RestPose() const -> Pose;

private:
    std::pmr::vector<JointSlot> m_Joints;
};

// Builds parent slots from the live ECS hierarchy; does not capture animated
// transforms as rest poses. All layout roots must be direct children of frame.
auto BuildJointLayout(std::span<const ecs::Entity>   entities,
                      std::span<const std::uint32_t> targets,
                      std::span<const LocalPose> rest, ecs::Entity frame = {}) -> JointLayout;

class JointBinding {
public:
    JointBinding(std::shared_ptr<const JointLayout> layout,
                 std::span<const ecs::Entity> entities, ecs::Entity frame = {});
    auto GetLayout() const noexcept -> const JointLayout& { return *m_Layout; }
    auto GetEntities() const noexcept -> std::span<const ecs::Entity> { return m_Entities; }
    void Validate() const;

private:
    std::shared_ptr<const JointLayout> m_Layout;
    std::pmr::vector<ecs::Entity>      m_Entities;
    ecs::Entity                        m_Frame;
};

// Missing channels preserve input. Use layout.RestPose() for deterministic
// full-pose sampling, or the previous pose for legacy sparse-channel semantics.
auto SamplePose(const AnimationClip& clip, double time, const JointLayout& layout,
                std::span<const LocalPose> input) -> Pose;
auto BlendPoses(std::span<const LocalPose> from, std::span<const LocalPose> to, float alpha) -> Pose;
auto ModelMatrices(const JointLayout& layout, std::span<const LocalPose> pose) -> std::pmr::vector<math::mat4f>;
void ValidatePose(std::span<const LocalPose> pose, std::size_t size);
// Continuous rigid root transform, including accumulated displacement across
// loops. Root scale must be identity. Times are unwrapped seconds.
auto SampleRootTransform(const AnimationClip& clip, std::uint32_t target, const LocalPose& rest,
                         double time, bool looping) -> math::mat4f;
auto ExtractRootMotion(const AnimationClip& clip, std::uint32_t target, const LocalPose& rest,
                       double from, double to, bool looping) -> math::mat4f;

// One base producer owns this output per actor; explicitly ordered rig stages may
// modify its pose before application. Root motion is a local rigid
// delta in the binding frame for the host controller; applying collision/world movement is not asset's job.
struct AnimationPose {
    explicit AnimationPose(JointBinding binding)
        : binding(std::move(binding)), pose(this->binding.GetLayout().RestPose()) {}
    JointBinding binding;
    Pose         pose;
    math::mat4f  root_motion = math::mat4f::identity();
};

struct PoseSystem {
    static void ConfigureSchedule(ecs::Schedule& schedule);
};

}  // namespace hitagi::asset

namespace hitagi::asset {

void ValidatePose(std::span<const LocalPose> pose, std::size_t size) {
    if (pose.size() != size) throw std::invalid_argument("Pose/layout size mismatch");
    for (const auto& joint : pose) {
        for (float v : joint.position.data)
            if (!std::isfinite(v)) throw std::invalid_argument("Non-finite pose position");
        for (float v : joint.scaling.data)
            if (!std::isfinite(v)) throw std::invalid_argument("Non-finite pose scale");
        for (float v : joint.rotation.data)
            if (!std::isfinite(v)) throw std::invalid_argument("Non-finite pose rotation");
        if (std::abs(joint.rotation.norm() - 1.0f) > 1e-4f)
            throw std::invalid_argument("Pose rotation must be normalized");
    }
}

JointLayout::JointLayout(std::span<const JointSlot> joints) : m_Joints(joints.begin(), joints.end()) {
    if (joints.empty()) throw std::invalid_argument("Joint layout must not be empty");
    std::pmr::unordered_set<std::uint32_t> targets;
    for (std::size_t i = 0; i < joints.size(); ++i) {
        const auto& joint = joints[i];
        if (joint.parent < -1 || joint.parent >= static_cast<std::int64_t>(i))
            throw std::invalid_argument("Joint parents must precede children");
        if (!targets.insert(joint.target).second) throw std::invalid_argument("Duplicate layout target");
        ValidatePose(std::span(&joint.rest, 1), 1);
    }
}

auto JointLayout::RestPose() const -> Pose {
    Pose pose;
    pose.reserve(m_Joints.size());
    for (const auto& joint : m_Joints) pose.push_back(joint.rest);
    return pose;
}

auto BuildJointLayout(std::span<const ecs::Entity> entities, std::span<const std::uint32_t> targets,
                      std::span<const LocalPose> rest, ecs::Entity frame) -> JointLayout {
    if (entities.size() != targets.size() || entities.size() != rest.size())
        throw std::invalid_argument("Joint layout input sizes differ");
    std::pmr::vector<JointSlot>          slots;
    std::pmr::unordered_set<ecs::Entity> seen;
    for (std::size_t i = 0; i < entities.size(); ++i) {
        const auto entity = entities[i];
        if (!entity || !entity.Has<Transform>() || !entity.Has<RelationShip>() || entity == frame || !seen.insert(entity).second)
            throw std::invalid_argument("Invalid or duplicate joint entity");
        const auto   parent      = entity.Get<RelationShip>().parent;
        std::int32_t parent_slot = -1;
        if (parent != frame) {
            const auto found = std::find(entities.begin(), entities.begin() + i, parent);
            if (found == entities.begin() + i) throw std::invalid_argument("Missing intermediate joint or parent order");
            parent_slot = static_cast<std::int32_t>(found - entities.begin());
        }
        slots.push_back({.target = targets[i], .parent = parent_slot, .rest = rest[i]});
    }
    return JointLayout(slots);
}

JointBinding::JointBinding(std::shared_ptr<const JointLayout> layout,
                           std::span<const ecs::Entity> entities, ecs::Entity frame)
    : m_Layout(std::move(layout)), m_Entities(entities.begin(), entities.end()), m_Frame(frame) {
    Validate();
}

void JointBinding::Validate() const {
    if (!m_Layout || m_Layout->GetJoints().size() != m_Entities.size())
        throw std::invalid_argument("Invalid joint binding size");
    // A non-default stale frame must not silently become an identity frame.
    if (m_Frame != ecs::Entity{} && (!m_Frame || !m_Frame.Has<Transform>() || !m_Frame.Has<RelationShip>()))
        throw std::invalid_argument("Joint binding frame is missing");
    std::pmr::unordered_set<ecs::Entity> seen;
    for (std::size_t i = 0; i < m_Entities.size(); ++i) {
        const auto joint = m_Entities[i];
        if (!joint || !joint.Has<Transform>() || !joint.Has<RelationShip>() || joint == m_Frame || !seen.insert(joint).second)
            throw std::invalid_argument("Invalid or duplicate bound joint");
        const auto parent = m_Layout->GetJoints()[i].parent;
        if (joint.Get<RelationShip>().parent != (parent < 0 ? m_Frame : m_Entities[parent]))
            throw std::invalid_argument("Joint hierarchy changed; rebuild animation binding");
    }
}

auto SamplePose(const AnimationClip& clip, double time, const JointLayout& layout,
                std::span<const LocalPose> input) -> Pose {
    ValidatePose(input, layout.GetJoints().size());
    if (!std::isfinite(time)) throw std::invalid_argument("Non-finite pose sample time");
    Pose result(input.begin(), input.end());
    for (std::size_t i = 0; i < result.size(); ++i)
        if (const auto* track = clip.FindTrack(layout.GetJoints()[i].target))
            result[i] = SampleAnimationTrack(*track, time, result[i]);
    return result;
}

auto BlendPoses(std::span<const LocalPose> from, std::span<const LocalPose> to, float alpha) -> Pose {
    ValidatePose(from, to.size());
    ValidatePose(to, from.size());
    Pose result;
    result.reserve(from.size());
    for (std::size_t i = 0; i < from.size(); ++i) result.push_back(BlendLocalPose(from[i], to[i], alpha));
    return result;
}

auto ModelMatrices(const JointLayout& layout, std::span<const LocalPose> pose) -> std::pmr::vector<math::mat4f> {
    ValidatePose(pose, layout.GetJoints().size());
    std::pmr::vector<math::mat4f> matrices;
    matrices.reserve(pose.size());
    for (std::size_t i = 0; i < pose.size(); ++i) {
        const auto parent = layout.GetJoints()[i].parent;
        matrices.push_back(parent < 0 ? pose[i].ToMatrix() : matrices[parent] * pose[i].ToMatrix());
    }
    return matrices;
}

void PoseSystem::ConfigureSchedule(ecs::Schedule& schedule) {
    schedule.Request("apply_animation_poses", [](ecs::Query<ecs::Entity, const AnimationPose&> query) {
        std::pmr::unordered_set<ecs::Entity> writers;
        // Validate all outputs before writing any pose. Entity handles borrow
        // their World; hosts must keep the same World alive throughout evaluation.
        query.Each([&](ecs::Entity owner, const AnimationPose& output) {
            output.binding.Validate();
            ValidatePose(output.pose, output.binding.GetEntities().size());
            for (const auto joint : output.binding.GetEntities()) {
                if (!owner.IsInSameWorld(joint)) throw std::invalid_argument("Pose joint belongs to another World");
                if (!writers.insert(joint).second || joint.Has<AnimationTarget>())
                    throw std::invalid_argument("Joint has multiple animation writers");
            }
        });
        query.Each([](ecs::Entity, const AnimationPose& output) {
            const auto entities = output.binding.GetEntities();
            for (std::size_t i = 0; i < entities.size(); ++i) {
                auto entity = entities[i];
                WriteLocalPose(output.pose[i], entity.Get<Transform>());
            }
        });
    });
    schedule.SetOrder("animate_targets", "apply_animation_poses");
    schedule.SetOrder("apply_animation_poses", "update_local_matrix");
}

auto SampleRootTransform(const AnimationClip& clip, std::uint32_t target, const LocalPose& rest,
                         double time, bool looping) -> math::mat4f {
    const auto* track  = clip.FindTrack(target);
    const auto  sample = [&](double t) {
        const auto pose = track ? SampleAnimationTrack(*track, t, rest) : rest;
        ValidatePose(std::span(&pose, 1), 1);
        if (pose.scaling != math::vec3f(1.0f)) throw std::invalid_argument("Root motion requires unit scale");
        return pose.ToMatrix();
    };
    const double duration = clip.GetDuration();
    const double wrapped  = ResolveAnimationTime(time, duration, looping);
    if (!looping || duration == 0.0) return sample(wrapped);
    const double cycles = std::floor(time / duration);
    if (!std::isfinite(cycles) || std::abs(cycles) > 1e9)
        throw std::invalid_argument("Root motion cycle count exceeds supported range");
    const auto start = sample(0.0);
    auto       cycle = sample(duration) * math::inverse(start);
    if (cycles < 0.0) cycle = math::inverse(cycle);
    auto count       = static_cast<std::uint64_t>(std::abs(cycles));
    auto accumulated = math::mat4f::identity();
    while (count) {
        if (count & 1) accumulated = accumulated * cycle;
        cycle = cycle * cycle;
        count >>= 1;
    }
    return accumulated * sample(wrapped);
}

auto ExtractRootMotion(const AnimationClip& clip, std::uint32_t target, const LocalPose& rest,
                       double from, double to, bool looping) -> math::mat4f {
    return math::inverse(SampleRootTransform(clip, target, rest, from, looping)) *
           SampleRootTransform(clip, target, rest, to, looping);
}

}  // namespace hitagi::asset
