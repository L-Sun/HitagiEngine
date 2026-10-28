#include "test_macros.hpp"
import interop.gtest;

import std;
import asset;
import core;
import math;
import ecs;
import test_utils;

using namespace testing;
using namespace hitagi;
using namespace hitagi::asset;

class TransformTest : public Test {
protected:
    TransformTest() : world("TransformTest"), em(world.GetEntityManager()), sm(world.GetSystemManager()) {}

    core::JobSystem     job_system;
    ecs::World          world;
    ecs::EntityManager& em;
    ecs::SystemManager& sm;
};

class RelationShipSystemTest : public TransformTest {
public:
    void SetUp() override {
        world.RegisterSystem<RelationShipSystem>();
    }
};

TEST_F(RelationShipSystemTest, UpdateChildren) {
    auto entities = em.CreateMany(3);

    entities[0].Emplace<RelationShip>();
    entities[1].Emplace<RelationShip>(entities[0]);  // set parent of entities[1] to entities[0]
    entities[2].Emplace<RelationShip>(entities[1]);  // set parent of entities[2] to entities[1]

    world.Update(job_system);

    EXPECT_EQ(entities[1].Get<RelationShip>().parent, entities[0])
        << "the parent of entities[1] should be entities[0] after update";

    EXPECT_EQ(entities[2].Get<RelationShip>().parent, entities[1])
        << "the parent of entities[2] should be entities[1] after update";

    ASSERT_TRUE(entities[0].Get<RelationShip>().GetChildren().contains(entities[1]))
        << "entities[1] should be in the children of entities[0] after update";

    ASSERT_TRUE(entities[1].Get<RelationShip>().GetChildren().contains(entities[2]))
        << "entities[2] should be in the children of entities[1] after update";
}

TEST_F(RelationShipSystemTest, AutoAttachChildren) {
    auto entities = em.CreateMany(2);

    entities[0].Emplace<RelationShip>();
    entities[1].Emplace<RelationShip>(entities[0]);  // set parent of entities[1] to entity_

    world.Update(job_system);

    ASSERT_TRUE(entities[0].Has<RelationShip>())
        << "entities[0] should be attached with Children component after update";

    EXPECT_TRUE(entities[0].Get<RelationShip>().GetChildren().contains(entities[1]))
        << "entities[1] should be in the children of entities[0] after update";
}

class LocalToWorldSystemTest : public TransformTest {
public:
    void SetUp() override {
        world.RegisterSystem<RelationShipSystem>();
        world.RegisterSystem<TransformSystem>();
    }
};

TEST_F(LocalToWorldSystemTest, UpdateLocalMatrix) {
    auto individual_entity = em.Create();  // Individual entity with transform
    individual_entity.Emplace<Transform>(math::vec3f{1.0f, 0.0f, 0.0f}, math::quatf::identity(), math::vec3f{1.0f, 1.0f, 1.0f});
    world.Update(job_system);

    EXPECT_EQ(individual_entity.Get<Transform>().world_matrix, math::translate(math::vec3f{1.0f, 0.0f, 0.0f}));
}

TEST_F(LocalToWorldSystemTest, UpdateHierarchy) {
    auto root_entity = em.Create();  // Root entity with transform and children

    root_entity.Emplace<Transform>(
        math::vec3f{2.0f, 0.0f, 0.0f},
        math::quatf::identity(),
        math::vec3f{1.0f, 1.0f, 1.0f});

    root_entity.Emplace<RelationShip>();

    auto child_entity = em.Create();  // Child entity with transform and a parent
    child_entity.Emplace<Transform>(
        math::vec3f{3.0f, 0.0f, 0.0f},
        math::quatf::identity(),
        math::vec3f{1.0f, 1.0f, 1.0f});

    child_entity.Emplace<RelationShip>(root_entity);  // Set parent of child_entity to root_entity

    world.Update(job_system);

    EXPECT_EQ(root_entity.Get<Transform>().world_matrix, math::translate(math::vec3f{2.0f, 0.0f, 0.0f}));
    EXPECT_EQ(child_entity.Get<Transform>().world_matrix, math::translate(math::vec3f{5.0f, 0.0f, 0.0f}));

    root_entity.Get<Transform>().Translate(math::vec3f{1.0f, 0.0f, 0.0f});
    world.Update(job_system);

    EXPECT_EQ(root_entity.Get<Transform>().world_matrix, math::translate(math::vec3f{3.0f, 0.0f, 0.0f}));
    EXPECT_EQ(child_entity.Get<Transform>().world_matrix, math::translate(math::vec3f{6.0f, 0.0f, 0.0f}));
}

TEST_F(LocalToWorldSystemTest, ReparentRebuildsHierarchyOrder) {
    auto root_a = em.Create();
    root_a.Emplace<Transform>(
        math::vec3f{1.0f, 0.0f, 0.0f},
        math::quatf::identity(),
        math::vec3f{1.0f, 1.0f, 1.0f});
    root_a.Emplace<RelationShip>();

    auto child = em.Create();
    child.Emplace<Transform>(
        math::vec3f{2.0f, 0.0f, 0.0f},
        math::quatf::identity(),
        math::vec3f{1.0f, 1.0f, 1.0f});
    child.Emplace<RelationShip>(root_a);

    auto grandchild = em.Create();
    grandchild.Emplace<Transform>(
        math::vec3f{3.0f, 0.0f, 0.0f},
        math::quatf::identity(),
        math::vec3f{1.0f, 1.0f, 1.0f});
    grandchild.Emplace<RelationShip>(child);

    auto root_b = em.Create();
    root_b.Emplace<Transform>(
        math::vec3f{10.0f, 0.0f, 0.0f},
        math::quatf::identity(),
        math::vec3f{1.0f, 1.0f, 1.0f});
    root_b.Emplace<RelationShip>();

    world.Update(job_system);

    EXPECT_EQ(child.Get<Transform>().world_matrix, math::translate(math::vec3f{3.0f, 0.0f, 0.0f}));
    EXPECT_EQ(grandchild.Get<Transform>().world_matrix, math::translate(math::vec3f{6.0f, 0.0f, 0.0f}));

    child.Get<RelationShip>().parent = root_b;
    world.Update(job_system);

    EXPECT_EQ(root_a.Get<Transform>().world_matrix, math::translate(math::vec3f{1.0f, 0.0f, 0.0f}));
    EXPECT_EQ(child.Get<Transform>().world_matrix, math::translate(math::vec3f{12.0f, 0.0f, 0.0f}));
    EXPECT_EQ(grandchild.Get<Transform>().world_matrix, math::translate(math::vec3f{15.0f, 0.0f, 0.0f}));
}

TEST_F(LocalToWorldSystemTest, DirectRootCreationAndRemovalInvalidateTraversal) {
    auto first = em.Create(Transform(math::vec3f(1, 0, 0)), RelationShip{});
    world.Update(job_system);
    auto second                        = em.Create(Transform(math::vec3f(2, 0, 0)), RelationShip{});
    second.Get<Transform>().position.x = 7;
    world.Update(job_system);
    EXPECT_EQ(second.Get<Transform>().world_matrix, math::translate(math::vec3f(7, 0, 0)));

    auto child = em.Create(Transform(math::vec3f(3, 0, 0)), RelationShip(second));
    world.Update(job_system);
    EXPECT_EQ(child.Get<Transform>().world_matrix, math::translate(math::vec3f(10, 0, 0)));
    em.Destroy(second);
    world.Update(job_system);
    EXPECT_EQ(child.Get<Transform>().world_matrix, math::translate(math::vec3f(3, 0, 0)));

    em.Destroy(first);
    em.Destroy(child);
    world.Update(job_system);
    auto replacement                        = em.Create(Transform{}, RelationShip{});
    replacement.Get<Transform>().position.x = 9;
    world.Update(job_system);
    EXPECT_EQ(replacement.Get<Transform>().world_matrix, math::translate(math::vec3f(9, 0, 0)));
}
TEST_F(LocalToWorldSystemTest, SparseChangesAndReparentingMatchFullHierarchyEvaluation) {
    // Parents precede children, but siblings share ancestors. Exercise sparse and
    // dense edits, detachment, and archetype relocation over multiple updates.
    std::vector<ecs::Entity> entities;
    std::vector<int> parents;
    for (int i = 0; i < 48; ++i) {
        const int parent = i == 0 ? -1 : (i - 1) / 2;
        parents.push_back(parent);
        entities.push_back(em.Create(Transform(math::vec3f(float(i % 5), 0, 0)),
                                     RelationShip(parent < 0 ? ecs::Entity{} : entities[parent])));
    }
    for (int frame = 0; frame < 32; ++frame) {
        for (int i = 0; i < 48; ++i) {
            if ((i + frame) % 7 == 0 || frame == 20)
                entities[i].Get<Transform>().position.x += 0.25f;
        }
        if (frame == 6 || frame == 12) {
            parents[8] = frame == 6 ? -1 : 2;
            entities[8].Get<RelationShip>().parent = parents[8] < 0 ? ecs::Entity{} : entities[parents[8]];
        }
        if (frame == 10) entities[5].Emplace<MetaInfo>("relocated");
        world.Update(job_system);
        std::vector<math::mat4f> expected;
        for (int i = 0; i < 48; ++i) {
            const auto& transform = entities[i].Get<Transform>();
            expected.push_back(parents[i] < 0 ? transform.ToMatrix() : expected[parents[i]] * transform.ToMatrix());
            EXPECT_EQ(transform.world_matrix, expected.back()) << "frame " << frame << ", entity " << i;
        }
    }
}
