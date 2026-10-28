#include "test_macros.hpp"
import interop.gtest;
import std;
import asset;
import core;
import ecs;
import math;
using namespace hitagi;
using namespace hitagi::asset;
namespace rig_test {
auto Layout() -> std::shared_ptr<const JointLayout> {
    const std::array slots{JointSlot{.target = 10},
                           JointSlot{.target = 20, .parent = 0, .rest = {.position = math::vec3f(0, 1, 0)}},
                           JointSlot{.target = 30, .parent = 1, .rest = {.position = math::vec3f(0, 1, 0)}},
                           JointSlot{.target = 40, .rest = {.position = math::vec3f(1, 1, 0)}}};
    return std::make_shared<JointLayout>(slots);
}
auto IK(std::shared_ptr<const JointLayout> layout) -> std::shared_ptr<const RigDefinition> {
    RigDesc desc;
    desc.controls = {{.id = {100}, .name = "tip"}, {.id = {200}, .name = "pole"}};
    desc.constraints.push_back({.id = {42}, .operation = TwoBoneIKConstraint{.root = {10}, .middle = {20}, .tip = {30}, .target = RigControlId{100}, .pole = RigControlId{200}}});
    return std::make_shared<RigDefinition>(layout, std::move(desc));
}
void Controls(RigInstance& rig, math::vec3f target, math::vec3f pole = math::vec3f(0, 0, 1)) {
    rig.SetControl({100}, {.model_transform = {.position = target}, .valid = true});
    rig.SetControl({200}, {.model_transform = {.position = pole}, .valid = true});
}
auto Point(const JointLayout& layout, const Pose& pose, unsigned slot) -> math::vec3f {
    return (ModelMatrices(layout, pose)[slot] * math::vec4f(0, 0, 0, 1)).xyz;
}
void Equal(const Pose& a, const Pose& b) {
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_LT((a[i].position - b[i].position).norm(), 1e-6f);
        EXPECT_LT((a[i].rotation - b[i].rotation).norm(), 1e-6f);
    }
}
}  // namespace rig_test
TEST(RigTest, TwoBoneReachesTargetAndPreservesBoneLengths) {
    const auto  layout = rig_test::Layout();
    RigInstance rig(rig_test::IK(layout));
    auto        input = layout->RestPose(), output = input;
    rig_test::Controls(rig, {1, 1, 0});
    auto report = rig.Evaluate({}, input, output);
    EXPECT_LT((rig_test::Point(*layout, output, 2) - math::vec3f(1, 1, 0)).norm(), 1e-4f);
    EXPECT_NEAR((rig_test::Point(*layout, output, 1) - rig_test::Point(*layout, output, 0)).norm(), 1, 1e-5f);
    EXPECT_NEAR((rig_test::Point(*layout, output, 2) - rig_test::Point(*layout, output, 1)).norm(), 1, 1e-5f);
    EXPECT_GT(rig_test::Point(*layout, output, 1).z, 0.1f);
    EXPECT_EQ(report[0].status, RigStatus::Solved);
    for (unsigned i = 0; i < input.size(); ++i) EXPECT_LT((input[i].position - output[i].position).norm(), 1e-7f);
    EXPECT_LT((input[3].rotation - output[3].rotation).norm(), 1e-7f);
}
TEST(RigTest, UnreachableAndCollinearTargetsStayFinite) {
    const auto  layout = rig_test::Layout();
    RigInstance rig(rig_test::IK(layout));
    auto        pose = layout->RestPose();
    rig_test::Controls(rig, {0, 4, 0}, {0, 2, 0});
    auto report = rig.Evaluate({}, pose, pose);
    EXPECT_EQ(report[0].status, RigStatus::Unreachable);
    EXPECT_NEAR(report[0].position_error, 2, 1e-5);
    EXPECT_NO_THROW(ValidatePose(pose, 4));
    rig_test::Controls(rig, {0, 0, 0}, {0, 0, 0});
    rig.Evaluate({}, layout->RestPose(), pose);
    EXPECT_NO_THROW(ValidatePose(pose, 4));
    EXPECT_LT(rig_test::Point(*layout, pose, 2).norm(), 1e-4);
}
TEST(RigTest, WeightsResetDisabledAndInstancesAreIndependent) {
    const auto  layout     = rig_test::Layout();
    const auto  definition = rig_test::IK(layout);
    RigInstance a(definition), b(definition);
    const auto  input  = layout->RestPose();
    auto        output = input;
    rig_test::Controls(a, {1, 1, 0});
    a.SetWeight({42}, 0);
    a.Evaluate({}, input, output);
    rig_test::Equal(input, output);
    a.SetWeight({42}, 1);
    a.enabled = false;
    a.Evaluate({}, input, output);
    rig_test::Equal(input, output);
    a.enabled = true;
    a.Evaluate({}, input, output);
    const auto first = output;
    a.Evaluate({.discontinuity = true}, input, output);
    rig_test::Equal(first, output);
    auto report = b.Evaluate({}, input, output);
    EXPECT_EQ(report[0].status, RigStatus::MissingTarget);
    rig_test::Equal(input, output);
    a.Reset();
    report = a.Evaluate({}, input, output);
    EXPECT_EQ(report[0].status, RigStatus::MissingTarget);
}
TEST(RigTest, WeightedResidualAndMirroredPoles) {
    const auto  layout = rig_test::Layout();
    RigInstance rig(rig_test::IK(layout));
    auto        output = layout->RestPose();
    rig_test::Controls(rig, {1, 1, 0}, {0, 0, -1});
    rig.SetWeight({42}, 0.5f);
    auto report = rig.Evaluate({}, layout->RestPose(), output);
    EXPECT_GT(report[0].position_error, 0.01f);
    rig.SetWeight({42}, 1);
    rig.Evaluate({}, layout->RestPose(), output);
    EXPECT_LT(rig_test::Point(*layout, output, 1).z, -0.1f);
    EXPECT_LT((rig_test::Point(*layout, output, 2) - math::vec3f(1, 1, 0)).norm(), 1e-4f);
}
TEST(RigTest, AimOppositeAndPoleDegeneracy) {
    const auto layout = rig_test::Layout();
    RigDesc    desc;
    desc.controls = {{.id = {7}}};
    desc.constraints.push_back({.id = {1}, .operation = AimConstraint{.joint = {10}, .target = RigControlId{7}}});
    RigInstance rig(std::make_shared<RigDefinition>(layout, desc));
    auto        output = layout->RestPose();
    rig.SetControl({7}, {.model_transform = {.position = {0, -1, 0}}, .valid = true});
    rig.Evaluate({}, layout->RestPose(), output);
    EXPECT_LT((rig_test::Point(*layout, output, 1) - math::vec3f(0, -1, 0)).norm(), 1e-5);
    rig.SetControl({7}, {.model_transform = {.position = {0, 0, 0}}, .valid = true});
    auto report = rig.Evaluate({}, layout->RestPose(), output);
    EXPECT_EQ(report[0].status, RigStatus::Degenerate);
    rig_test::Equal(layout->RestPose(), output);
}
TEST(RigTest, OrderedConstraintsSeeUpdatedModelCache) {
    const auto layout = rig_test::Layout();
    RigDesc    desc;
    desc.constraints.push_back({.id = {1}, .operation = AimConstraint{.joint = {10}, .target = RigJointTarget{.joint = {40}}}});
    desc.constraints.push_back({.id = {2}, .operation = AimConstraint{.joint = {20}, .target = RigJointTarget{.joint = {40}}}});
    RigInstance rig(std::make_shared<RigDefinition>(layout, desc));
    auto        pose = layout->RestPose();
    rig.Evaluate({}, pose, pose);
    const auto from = rig_test::Point(*layout, pose, 2) - rig_test::Point(*layout, pose, 1);
    const auto to   = rig_test::Point(*layout, pose, 3) - rig_test::Point(*layout, pose, 1);
    EXPECT_GT(math::dot(math::normalize(from), math::normalize(to)), 0.99999f);
}
TEST(RigTest, RejectsInvalidDefinitionInputAndLayout) {
    const auto layout = rig_test::Layout();
    RigDesc    desc;
    desc.constraints.push_back({.id = {1}, .operation = AimConstraint{.joint = {999}, .target = RigJointTarget{.joint = {40}}}});
    EXPECT_THROW(RigDefinition(layout, desc), std::invalid_argument);
    RigInstance rig(rig_test::IK(layout));
    auto        pose  = layout->RestPose();
    pose[0].scaling.x = 2;
    EXPECT_THROW(rig.Evaluate({}, pose, pose), std::invalid_argument);
    EXPECT_THROW(rig.SetWeight({42}, -1), std::invalid_argument);
    EXPECT_THROW(rig.SetControl({999}, {}), std::invalid_argument);
    auto slots               = std::pmr::vector<JointSlot>(layout->GetJoints().begin(), layout->GetJoints().end());
    slots[1].rest.position.y = 2;
    EXPECT_THROW(rig.GetDefinition().ValidateLayout(JointLayout(slots)), std::invalid_argument);
}
TEST(RigTest, ECSAppliesRigBeforeBoneTransforms) {
    core::Runtime               runtime({.worker_count = 2, .log_level = "warn"});
    Scene                       scene("rig");
    auto&                       world = scene.GetWorld();
    core::PluginSet<ecs::World> plugins;
    plugins.Add(world, RigPlugin{});
    const auto                    layout   = rig_test::Layout();
    auto&                         entities = world.GetEntityManager();
    auto                          actor    = entities.Create(Transform{}, RelationShip{});
    std::pmr::vector<ecs::Entity> joints;
    for (const auto& j : layout->GetJoints()) joints.push_back(entities.Create(
        Transform(j.rest.position, j.rest.rotation, j.rest.scaling), RelationShip(j.parent < 0 ? actor : joints[j.parent])));
    actor.Emplace<AnimationPose>(JointBinding(layout, joints, actor));
    auto& rig = actor.Emplace<RigInstance>(rig_test::IK(layout));
    rig_test::Controls(rig, {1, 1, 0});
    world.Update(runtime.Jobs(), 1.0 / 60);
    EXPECT_LT((rig_test::Point(*layout, actor.Get<AnimationPose>().pose, 2) - math::vec3f(1, 1, 0)).norm(), 1e-4);
}

TEST(RigTest, AimPoleControlsTwistAndMissingPoleDoesNotWrite) {
    const auto layout = rig_test::Layout();
    RigDesc    desc;
    desc.controls = {{.id = {1}}, {.id = {2}}};
    desc.constraints.push_back({.id = {5}, .operation = AimConstraint{.joint = {10}, .target = RigControlId{1}, .pole = RigTarget(RigControlId{2})}});
    RigInstance rig(std::make_shared<RigDefinition>(layout, desc));
    auto        pose = layout->RestPose();
    rig.SetControl({1}, {.model_transform = {.position = {0, 2, 0}}, .valid = true});
    rig.Evaluate({}, layout->RestPose(), pose);
    rig_test::Equal(layout->RestPose(), pose);
    rig.SetControl({2}, {.model_transform = {.position = {1, 0, 0}}, .valid = true});
    rig.Evaluate({}, layout->RestPose(), pose);
    const math::vec3f up = (math::rotate(pose[0].rotation) * math::vec4f(0, 0, 1, 0)).xyz;
    EXPECT_LT((up - math::vec3f(1, 0, 0)).norm(), 1e-5);
    rig.SetControl({2}, {.model_transform = {.position = {0, 1, 0}}, .valid = true});
    const auto report = rig.Evaluate({}, layout->RestPose(), pose);
    EXPECT_EQ(report[0].status, RigStatus::Degenerate);
    rig_test::Equal(layout->RestPose(), pose);
}

TEST(RigTest, TipOrientationWithNonidentityRoot) {
    const auto layout = rig_test::Layout();
    RigDesc    desc;
    desc.controls = {{.id = {100}}, {.id = {200}}};
    desc.constraints.push_back({.id = {42}, .operation = TwoBoneIKConstraint{.root = {10}, .middle = {20}, .tip = {30}, .target = RigControlId{100}, .pole = RigControlId{200}, .rotation_weight = 1}});
    RigInstance rig(std::make_shared<RigDefinition>(layout, desc));
    auto        input = layout->RestPose(), output = input;
    input[0].position      = {2, 0, 3};
    input[0].rotation      = math::axis_angle_to_quaternion(math::vec3f(0, 1, 0), 1.0f);
    const auto orientation = math::axis_angle_to_quaternion(math::vec3f(1, 0, 0), 0.7f);
    rig.SetControl({100}, {.model_transform = {.position = {3, 1, 3}, .rotation = orientation}, .valid = true});
    rig.SetControl({200}, {.model_transform = {.position = {2, 0, 4}}, .valid = true});
    const auto report = rig.Evaluate({}, input, output);
    EXPECT_LT(report[0].position_error, 1e-4f);
    EXPECT_LT(report[0].angular_error, 0.001f);
    EXPECT_LT((output[0].position - input[0].position).norm(), 1e-6f);
}
TEST(RigTest, RejectsSelfDependentTargetsAndDuplicateIds) {
    const auto layout = rig_test::Layout();
    RigDesc    desc;
    desc.constraints.push_back({.id = {1}, .operation = AimConstraint{.joint = {10}, .target = RigJointTarget{.joint = {30}}}});
    EXPECT_THROW(RigDefinition(layout, desc), std::invalid_argument);
    desc.constraints.clear();
    desc.controls = {{.id = {1}}, {.id = {1}}};
    EXPECT_THROW(RigDefinition(layout, desc), std::invalid_argument);
}

TEST(RigTest, ImportedHingeLocksSwingAndClampsSignedAngle) {
    const auto layout = rig_test::Layout();
    RigDesc    desc;
    desc.controls = {{.id = {100}}, {.id = {200}}};
    desc.constraints.push_back({.id = {42}, .operation = TwoBoneIKConstraint{.root = {10}, .middle = {20}, .tip = {30}, .target = RigControlId{100}, .pole = RigControlId{200}, .middle_hinge = RigHingeLimit{.axis = {0, 0, 1}, .minimum = 0.3f, .maximum = 0.8f}}});
    RigInstance rig(std::make_shared<RigDefinition>(layout, desc));
    auto        pose = layout->RestPose();
    rig_test::Controls(rig, {0, 3, 0}, {1, 0, 0});
    const auto report = rig.Evaluate({}, pose, pose);
    EXPECT_EQ(report[0].status, RigStatus::Unreachable);
    EXPECT_NEAR(pose[1].rotation.x, 0, 1e-6);
    EXPECT_NEAR(pose[1].rotation.y, 0, 1e-6);
    const float angle = 2 * std::atan2(pose[1].rotation.z, pose[1].rotation.w);
    EXPECT_GE(angle, 0.3f - 1e-5f);
    EXPECT_LE(angle, 0.8f + 1e-5f);
    EXPECT_NEAR((rig_test::Point(*layout, pose, 2) - rig_test::Point(*layout, pose, 1)).norm(), 1, 1e-5);
    auto& op                 = std::get<TwoBoneIKConstraint>(desc.constraints[0].operation);
    op.middle_hinge->minimum = 2;
    op.middle_hinge->maximum = 1;
    EXPECT_THROW(RigDefinition(layout, desc), std::invalid_argument);
}

TEST(RigTest, SolvedControlsDriveSeparateDeformationBones) {
    const auto layout = rig_test::Layout();
    RigDesc    desc;
    desc.constraints.push_back({.id = {1}, .operation = AimConstraint{.joint = {10}, .target = RigJointTarget{.joint = {40}}}});
    desc.constraints.push_back({.id = {2}, .operation = CopyLocalRotationConstraint{.joint = {40}, .source = {10}}});
    RigInstance rig(std::make_shared<RigDefinition>(layout, desc));
    auto        pose               = layout->RestPose();
    const auto  unchanged_position = pose[3].position;
    rig.Evaluate({}, pose, pose);
    EXPECT_LT((pose[3].rotation - pose[0].rotation).norm(), 1e-6);
    EXPECT_GT((pose[3].rotation - math::quatf::identity()).norm(), 0.1);
    EXPECT_LT((pose[3].position - unchanged_position).norm(), 1e-6);
    rig.SetWeight({2}, 0);
    rig.Evaluate({}, layout->RestPose(), pose);
    EXPECT_LT((pose[3].rotation - math::quatf::identity()).norm(), 1e-6);
}
