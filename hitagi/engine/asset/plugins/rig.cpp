export module asset.plugins:rig;
import std;
import math;
import ecs;
import asset.base;

export namespace hitagi::asset {
struct RigJointId {
    std::uint32_t value = 0;
};
struct RigControlId {
    std::uint32_t value = 0;
};
struct RigConstraintId {
    std::uint32_t value = 0;
};
struct RigControlDesc {
    RigControlId     id;
    std::pmr::string name;
};
struct RigJointTarget {
    RigJointId joint;
    LocalPose  offset;
};
using RigTarget = std::variant<RigControlId, RigJointTarget>;
struct AimConstraint {
    RigJointId               joint;
    math::vec3f              local_aim_axis{0, 1, 0};
    math::vec3f              local_up_axis{0, 0, 1};
    RigTarget                target;
    std::optional<RigTarget> pole;
    float                    weight = 1;
};
// Signed twist interval around an axis in the middle joint's local pose space.
// Orthogonal swing is locked. This is a hinge, not arbitrary Euler limits.
struct RigHingeLimit {
    math::vec3f axis{1, 0, 0};
    float       minimum = 0;
    float       maximum = std::numbers::pi_v<float>;
};
struct TwoBoneIKConstraint {
    RigJointId                   root, middle, tip;
    RigTarget                    target, pole;
    float                        position_weight = 1;
    float                        rotation_weight = 0;
    std::optional<RigHingeLimit> middle_hinge;
    std::uint32_t                iterations = 200;
    float                        tolerance  = 1e-5f;
};
// Copy in a common local pose basis; no retargeting or implicit axis conversion.
struct CopyLocalRotationConstraint {
    RigJointId joint, source;
    float      weight = 1;
};
using RigOperation = std::variant<AimConstraint, TwoBoneIKConstraint, CopyLocalRotationConstraint>;
struct RigConstraint {
    RigConstraintId id;
    RigOperation    operation;
};
struct RigDesc {
    std::pmr::vector<RigControlDesc> controls;
    std::pmr::vector<RigConstraint>  constraints;
};
// Immutable, layout-specific CPU definition. Constructor validates and resolves
// logical joint/control IDs to slots. No ECS handles or cooked format yet.
class RigDefinition {
public:
    RigDefinition(std::shared_ptr<const JointLayout> layout, RigDesc desc);
    auto GetLayout() const -> const JointLayout& { return *m_Layout; }
    void ValidateLayout(const JointLayout& layout) const;

private:
    friend class RigInstance;
    std::shared_ptr<const JointLayout> m_Layout;
    RigDesc                            m_Desc;
};
struct RigControlValue {
    LocalPose model_transform;
    bool      valid = false;
};
enum class RigStatus { Solved,
                       Skipped,
                       MissingTarget,
                       Degenerate,
                       Unreachable };
struct RigDiagnostic {
    RigConstraintId constraint;
    RigStatus       status         = RigStatus::Skipped;
    float           position_error = 0;  // model units; Aim uses angular_error instead
    float           angular_error  = 0;  // radians, after weighting
};
struct RigEvaluationContext {
    double delta_seconds = 0;
    bool   discontinuity = false;
};
// Each instance owns its controls, weights and work buffers. Evaluate can alias
// input/output, but callers must supply a fresh base pose on every evaluation.
class RigInstance {
public:
    explicit RigInstance(std::shared_ptr<const RigDefinition> definition);
    void SetControl(RigControlId id, RigControlValue value);
    void SetWeight(RigConstraintId id, float weight);
    void Reset();  // clears controls and reports; retains authored/overridden weights
    auto GetDefinition() const -> const RigDefinition& { return *m_Definition; }
    auto Evaluate(RigEvaluationContext context, std::span<const LocalPose> input,
                  std::span<LocalPose> output) -> std::span<const RigDiagnostic>;
    auto GetDiagnostics() const -> std::span<const RigDiagnostic> { return m_Report; }
    bool enabled = true;

private:
    void                                 UpdateModels(std::optional<std::uint32_t> changed = {});
    auto                                 Point(std::uint32_t slot) const -> math::vec3f;
    auto                                 Rotation(std::uint32_t slot) const -> math::quatf;
    auto                                 Target(const RigTarget& target) const -> std::optional<LocalPose>;
    void                                 RotateGlobal(std::uint32_t slot, math::quatf rotation, float weight);
    std::shared_ptr<const RigDefinition> m_Definition;
    std::pmr::vector<RigControlValue>    m_Controls;
    std::pmr::vector<float>              m_Weights;
    Pose                                 m_Work;
    std::pmr::vector<math::mat4f>        m_Models;
    std::pmr::vector<std::uint8_t>       m_Dirty;
    std::pmr::vector<RigDiagnostic>      m_Report;
};
struct RigSystem {
    static void ConfigureSchedule(ecs::Schedule& schedule);
};
struct RigPlugin {
    void Build(ecs::World& world) const { world.RegisterSystem<RigSystem>(); }
};
}  // namespace hitagi::asset

namespace hitagi::asset::rig_detail {
using math::quatf;
using math::vec3f;
auto Unit(quatf q) -> quatf {
    const float n = q.norm();
    return quatf(q.x / n, q.y / n, q.z / n, q.w / n);
}
auto Inverse(quatf q) -> quatf { return quatf(-q.x, -q.y, -q.z, q.w); }
auto Direction(quatf q, vec3f v) -> vec3f { return (math::rotate(q) * math::vec4f(v, 0)).xyz; }
auto Perpendicular(vec3f v) -> vec3f {
    return math::normalize(math::cross(v, std::abs(v.x) < 0.9f ? vec3f(1, 0, 0) : vec3f(0, 1, 0)));
}
auto FromTo(vec3f from, vec3f to) -> quatf {
    from          = math::normalize(from);
    to            = math::normalize(to);
    const float c = std::clamp(math::dot(from, to), -1.0f, 1.0f);
    if (c < -0.99999f) return math::axis_angle_to_quaternion(Perpendicular(from), std::numbers::pi_v<float>);
    const auto a = math::cross(from, to);
    return Unit(quatf(a.x, a.y, a.z, 1 + c));
}
void Weight(float weight) {
    if (!std::isfinite(weight) || weight < 0 || weight > 1) throw std::invalid_argument("Rig weight must be in [0,1]");
}
void Axis(vec3f v) {
    for (float x : v.data)
        if (!std::isfinite(x)) throw std::invalid_argument("Non-finite rig axis");
    if (v.norm() < 1e-6f) throw std::invalid_argument("Zero rig axis");
}
void Rigid(std::span<const LocalPose> pose) {
    ValidatePose(pose, pose.size());
    for (const auto& p : pose)
        for (float s : p.scaling.data)
            if (std::abs(s - 1) > 1e-5f) throw std::invalid_argument("Rig requires unit joint scales; scale the binding frame instead");
}
}  // namespace hitagi::asset::rig_detail
namespace hitagi::asset {
RigDefinition::RigDefinition(std::shared_ptr<const JointLayout> layout, RigDesc desc)
    : m_Layout(std::move(layout)), m_Desc(std::move(desc)) {
    if (!m_Layout) throw std::invalid_argument("Null rig layout");
    const auto joints = m_Layout->GetJoints();
    rig_detail::Rigid(m_Layout->RestPose());
    auto joint = [&](RigJointId& id) {
        for (std::uint32_t i = 0; i < joints.size(); ++i)
            if (joints[i].target == id.value) {
                id.value = i;
                return;
            }
        throw std::invalid_argument("Unknown rig joint ID");
    };
    std::pmr::unordered_set<std::uint32_t> ids;
    for (const auto& c : m_Desc.controls)
        if (!ids.insert(c.id.value).second) throw std::invalid_argument("Duplicate rig control ID");
    ids.clear();
    auto target = [&](RigTarget& t) {
        if (auto* c = std::get_if<RigControlId>(&t)) {
            for (std::uint32_t i = 0; i < m_Desc.controls.size(); ++i)
                if (m_Desc.controls[i].id.value == c->value) {
                    c->value = i;
                    return;
                }
            throw std::invalid_argument("Unknown rig control");
        }
        auto& j = std::get<RigJointTarget>(t);
        joint(j.joint);
        rig_detail::Rigid(std::span(&j.offset, 1));
    };
    auto outside = [&](const RigTarget& t, std::uint32_t pivot) {
        if (const auto* j = std::get_if<RigJointTarget>(&t))
            for (auto slot = static_cast<std::int32_t>(j->joint.value); slot >= 0; slot = joints[slot].parent)
                if (slot == static_cast<std::int32_t>(pivot)) throw std::invalid_argument("Rig target depends on its own solved subtree");
    };
    for (auto& c : m_Desc.constraints) {
        if (!ids.insert(c.id.value).second) throw std::invalid_argument("Duplicate rig constraint ID");
        std::visit([&](auto& op) {
            if constexpr (!std::is_same_v<std::decay_t<decltype(op)>, CopyLocalRotationConstraint>) target(op.target);
            if constexpr (std::is_same_v<std::decay_t<decltype(op)>, AimConstraint>) {
                joint(op.joint);
                rig_detail::Axis(op.local_aim_axis);
                rig_detail::Axis(op.local_up_axis);
                op.local_aim_axis = math::normalize(op.local_aim_axis);
                op.local_up_axis  = math::normalize(op.local_up_axis);
                if (math::cross(op.local_aim_axis, op.local_up_axis).norm() < 1e-5f) throw std::invalid_argument("Collinear rig axes");
                rig_detail::Weight(op.weight);
                outside(op.target, op.joint.value);
                if (op.pole) {
                    target(*op.pole);
                    outside(*op.pole, op.joint.value);
                }
            } else if constexpr (std::is_same_v<std::decay_t<decltype(op)>, CopyLocalRotationConstraint>) {
                joint(op.joint);
                joint(op.source);
                rig_detail::Weight(op.weight);
                if (op.joint.value == op.source.value) throw std::invalid_argument("Self-referencing rotation copy");
            } else {
                joint(op.root);
                joint(op.middle);
                joint(op.tip);
                target(op.pole);
                outside(op.target, op.root.value);
                outside(op.pole, op.root.value);
                rig_detail::Weight(op.position_weight);
                rig_detail::Weight(op.rotation_weight);
                if (op.iterations == 0 || op.iterations > 2048 || !std::isfinite(op.tolerance) || op.tolerance <= 0)
                    throw std::invalid_argument("Invalid IK iteration settings");
                if (op.middle_hinge) {
                    auto& hinge = *op.middle_hinge;
                    rig_detail::Axis(hinge.axis);
                    hinge.axis = math::normalize(hinge.axis);
                    if (!std::isfinite(hinge.minimum) || !std::isfinite(hinge.maximum) ||
                        hinge.minimum > hinge.maximum || hinge.minimum < -std::numbers::pi_v<float> ||
                        hinge.maximum > std::numbers::pi_v<float>)
                        throw std::invalid_argument("Invalid IK hinge interval");
                }
                if (joints[op.middle.value].parent != static_cast<int>(op.root.value) || joints[op.tip.value].parent != static_cast<int>(op.middle.value))
                    throw std::invalid_argument("TwoBoneIK requires a direct three-joint chain");
                if (joints[op.middle.value].rest.position.norm() < 1e-6f || joints[op.tip.value].rest.position.norm() < 1e-6f)
                    throw std::invalid_argument("Zero-length IK bone");
            }
        },
                   c.operation);
    }
}
void RigDefinition::ValidateLayout(const JointLayout& layout) const {
    const auto a = m_Layout->GetJoints(), b = layout.GetJoints();
    if (a.size() != b.size()) throw std::invalid_argument("Rig layout size mismatch");
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].target != b[i].target || a[i].parent != b[i].parent) throw std::invalid_argument("Rig topology mismatch");
        const auto x = a[i].rest.ToMatrix(), y = b[i].rest.ToMatrix();
        for (unsigned c = 0; c < 4; ++c)
            for (unsigned r = 0; r < 4; ++r)
                if (std::abs(x[c][r] - y[c][r]) > 1e-5f) throw std::invalid_argument("Rig rest pose mismatch");
    }
}
RigInstance::RigInstance(std::shared_ptr<const RigDefinition> definition) : m_Definition(std::move(definition)) {
    if (!m_Definition) throw std::invalid_argument("Null rig definition");
    m_Controls.resize(m_Definition->m_Desc.controls.size());
    m_Weights.resize(m_Definition->m_Desc.constraints.size(), 1);
    m_Report.resize(m_Weights.size());
    m_Work = m_Definition->GetLayout().RestPose();
    m_Models.resize(m_Work.size());
    m_Dirty.resize(m_Work.size());
}
void RigInstance::SetControl(RigControlId id, RigControlValue value) {
    if (value.valid) rig_detail::Rigid(std::span(&value.model_transform, 1));
    const auto& controls = m_Definition->m_Desc.controls;
    for (std::size_t i = 0; i < controls.size(); ++i)
        if (controls[i].id.value == id.value) {
            m_Controls[i] = value;
            return;
        }
    throw std::invalid_argument("Unknown rig control ID");
}
void RigInstance::SetWeight(RigConstraintId id, float weight) {
    rig_detail::Weight(weight);
    const auto& constraints = m_Definition->m_Desc.constraints;
    for (std::size_t i = 0; i < constraints.size(); ++i)
        if (constraints[i].id.value == id.value) {
            m_Weights[i] = weight;
            return;
        }
    throw std::invalid_argument("Unknown rig constraint ID");
}
void RigInstance::Reset() {
    for (auto& c : m_Controls) c.valid = false;
    for (auto& r : m_Report) r = {};
}
void RigInstance::UpdateModels(std::optional<std::uint32_t> changed) {
    const auto joints = m_Definition->GetLayout().GetJoints();
    for (std::size_t i = 0; i < m_Work.size(); ++i) {
        const auto parent = joints[i].parent;
        m_Dirty[i]        = !changed || i == *changed || (parent >= 0 && m_Dirty[parent]);
        if (m_Dirty[i]) m_Models[i] = parent < 0 ? m_Work[i].ToMatrix() : m_Models[parent] * m_Work[i].ToMatrix();
    }
}
auto RigInstance::Point(std::uint32_t slot) const -> math::vec3f { return (m_Models[slot] * math::vec4f(0, 0, 0, 1)).xyz; }
auto RigInstance::Rotation(std::uint32_t slot) const -> math::quatf { return std::get<1>(math::decompose(m_Models[slot])); }
auto RigInstance::Target(const RigTarget& target) const -> std::optional<LocalPose> {
    if (const auto* c = std::get_if<RigControlId>(&target)) {
        const auto& v = m_Controls[c->value];
        return v.valid ? std::optional(v.model_transform) : std::nullopt;
    }
    const auto& j        = std::get<RigJointTarget>(target);
    const auto [p, q, s] = math::decompose(m_Models[j.joint.value] * j.offset.ToMatrix());
    return LocalPose{.position = p, .rotation = q, .scaling = s};
}
void RigInstance::RotateGlobal(std::uint32_t slot, math::quatf rotation, float weight) {
    const auto parent = m_Definition->GetLayout().GetJoints()[slot].parent;
    auto       local  = m_Work[slot];
    local.rotation    = rig_detail::Unit(parent < 0 ? rotation : rig_detail::Inverse(Rotation(parent)) * rotation);
    m_Work[slot]      = BlendLocalPose(m_Work[slot], local, weight);
    UpdateModels(slot);
}
auto RigInstance::Evaluate(RigEvaluationContext context, std::span<const LocalPose> input, std::span<LocalPose> output)
    -> std::span<const RigDiagnostic> {
    if (!std::isfinite(context.delta_seconds) || context.delta_seconds < 0) throw std::invalid_argument("Invalid rig delta");
    if (input.size() != m_Work.size() || output.size() != m_Work.size()) throw std::invalid_argument("Rig pose size mismatch");
    rig_detail::Rigid(input);
    std::copy(input.begin(), input.end(), m_Work.begin());
    UpdateModels();
    // Current analytic solvers have no temporal history; discontinuity needs no extra work.
    for (std::size_t i = 0; i < m_Report.size(); ++i) {
        const auto& entry  = m_Definition->m_Desc.constraints[i];
        auto&       report = m_Report[i];
        report             = {.constraint = entry.id};
        if (!enabled || m_Weights[i] == 0) continue;
        std::visit([&](const auto& op) {
            if constexpr (std::is_same_v<std::decay_t<decltype(op)>, CopyLocalRotationConstraint>) {
                const float weight = m_Weights[i] * op.weight;
                if (weight == 0) return;
                auto local             = m_Work[op.joint.value];
                local.rotation         = m_Work[op.source.value].rotation;
                m_Work[op.joint.value] = BlendLocalPose(m_Work[op.joint.value], local, weight);
                UpdateModels(op.joint.value);
                report.status = RigStatus::Solved;
            } else {
                if constexpr (std::is_same_v<std::decay_t<decltype(op)>, AimConstraint>) {
                    if (op.weight == 0) return;
                } else if (op.position_weight == 0 && op.rotation_weight == 0)
                    return;
                const auto target = Target(op.target);
                if (!target) {
                    report.status = RigStatus::MissingTarget;
                    return;
                }
                if constexpr (std::is_same_v<std::decay_t<decltype(op)>, AimConstraint>) {
                    const float weight = m_Weights[i] * op.weight;
                    if (weight == 0) return;
                    auto direction = target->position - Point(op.joint.value);
                    if (direction.norm() < 1e-6f) {
                        report.status = RigStatus::Degenerate;
                        return;
                    }
                    direction    = math::normalize(direction);
                    auto desired = rig_detail::FromTo(rig_detail::Direction(Rotation(op.joint.value), op.local_aim_axis), direction) * Rotation(op.joint.value);
                    if (op.pole) {
                        const auto pole = Target(*op.pole);
                        if (!pole) {
                            report.status = RigStatus::MissingTarget;
                            return;
                        }
                        auto up = pole->position - Point(op.joint.value);
                        up -= direction * math::dot(up, direction);
                        auto old = rig_detail::Direction(desired, op.local_up_axis);
                        old -= direction * math::dot(old, direction);
                        if (up.norm() < 1e-6f || old.norm() < 1e-6f) {
                            report.status = RigStatus::Degenerate;
                            return;
                        }
                        up                = math::normalize(up);
                        old               = math::normalize(old);
                        const float angle = std::atan2(math::dot(direction, math::cross(old, up)), math::dot(old, up));
                        desired           = math::axis_angle_to_quaternion(direction, angle) * desired;
                    }
                    RotateGlobal(op.joint.value, desired, weight);
                    const auto actual    = rig_detail::Direction(Rotation(op.joint.value), op.local_aim_axis);
                    report.angular_error = std::acos(std::clamp(math::dot(actual, direction), -1.0f, 1.0f));
                    report.status        = RigStatus::Solved;
                } else {
                    const auto pole = Target(op.pole);
                    if (!pole) {
                        report.status = RigStatus::MissingTarget;
                        return;
                    }
                    const auto  root = op.root.value, mid = op.middle.value, tip = op.tip.value;
                    const auto  r = Point(root), m = Point(mid), t = Point(tip);
                    const float a = (m - r).norm(), b = (t - m).norm();
                    auto        direction = target->position - r;
                    const float distance  = direction.norm();
                    if (a < 1e-6f || b < 1e-6f) {
                        report.status = RigStatus::Degenerate;
                        return;
                    }
                    if (distance < 1e-6f)
                        direction = (t - r).norm() > 1e-6f ? math::normalize(t - r) : math::normalize(m - r);
                    else
                        direction /= distance;
                    auto bend = pole->position - r;
                    bend -= direction * math::dot(bend, direction);
                    if (bend.norm() < 1e-6f) {
                        bend = m - r;
                        bend -= direction * math::dot(bend, direction);
                    }
                    if (bend.norm() < 1e-6f) bend = rig_detail::Perpendicular(direction);
                    bend                   = math::normalize(bend);
                    const float d          = std::clamp(distance, std::max(std::abs(a - b), 1e-6f), a + b);
                    const float x          = (a * a - b * b + d * d) / (2 * d);
                    const auto  wanted_mid = r + direction * x + bend * std::sqrt(std::max(0.0f, a * a - x * x));
                    const auto  wanted_tip = r + direction * d;
                    const auto  old_root = m_Work[root], old_mid = m_Work[mid], old_tip = m_Work[tip];
                    RotateGlobal(root, rig_detail::FromTo(m - r, wanted_mid - r) * Rotation(root), 1);
                    RotateGlobal(mid, rig_detail::FromTo(Point(tip) - Point(mid), wanted_tip - Point(mid)) * Rotation(mid), 1);
                    if (op.middle_hinge) {
                        const auto& hinge     = *op.middle_hinge;
                        auto        constrain = [&] {
                            auto q = m_Work[mid].rotation;
                            if (q.w < 0) q = math::quatf(-q.x, -q.y, -q.z, -q.w);
                            const float angle    = std::clamp(2 * std::atan2(math::dot(math::vec3f(q.x, q.y, q.z), hinge.axis), q.w),
                                                              hinge.minimum, hinge.maximum);
                            m_Work[mid].rotation = math::axis_angle_to_quaternion(hinge.axis, angle);
                            UpdateModels(mid);
                        };
                        constrain();
                        // Projected CCD refines the analytic seed under the imported hinge.
                        for (std::uint32_t iteration = 0; iteration < op.iterations; ++iteration) {
                            for (auto pivot : {mid, root}) {
                                const auto from = Point(tip) - Point(pivot), to = target->position - Point(pivot);
                                if (from.norm() < 1e-6f || to.norm() < 1e-6f) continue;
                                RotateGlobal(pivot, rig_detail::FromTo(from, to) * Rotation(pivot), 1);
                                if (pivot == mid) constrain();
                            }
                            if ((Point(tip) - target->position).norm() < op.tolerance) break;
                        }
                    }
                    const auto solved_root = m_Work[root], solved_mid = m_Work[mid];
                    m_Work[root] = BlendLocalPose(old_root, solved_root, m_Weights[i] * op.position_weight);
                    m_Work[mid]  = BlendLocalPose(old_mid, solved_mid, m_Weights[i] * op.position_weight);
                    m_Work[tip]  = old_tip;
                    UpdateModels(root);
                    RotateGlobal(tip, target->rotation, m_Weights[i] * op.rotation_weight);
                    report.position_error = (Point(tip) - target->position).norm();
                    const auto error      = rig_detail::Inverse(target->rotation) * Rotation(tip);
                    report.angular_error  = 2 * std::acos(std::clamp(std::abs(error.w), 0.0f, 1.0f));
                    report.status         = distance > a + b + 1e-5f || distance < std::abs(a - b) - 1e-5f ? RigStatus::Unreachable : RigStatus::Solved;
                }
            }
        },
                   entry.operation);
    }
    ValidatePose(m_Work, m_Work.size());
    std::copy(m_Work.begin(), m_Work.end(), output.begin());
    return m_Report;
}
void RigSystem::ConfigureSchedule(ecs::Schedule& schedule) {
    schedule.Request("solve_rigs", [](ecs::Query<AnimationPose&, RigInstance&> query) {
        query.Each([](AnimationPose& pose, RigInstance& rig) {
            rig.GetDefinition().ValidateLayout(pose.binding.GetLayout());
            rig.Evaluate({}, pose.pose, pose.pose);
        });
    });
    schedule.SetOrder("solve_rigs", "apply_animation_poses");
}
}  // namespace hitagi::asset
