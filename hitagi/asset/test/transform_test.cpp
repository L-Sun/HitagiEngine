#include <hitagi/asset/transform.hpp>
#include <hitagi/utils/test.hpp>

using namespace testing;
using namespace hitagi;
using namespace hitagi::asset;

class TransformTest : public Test {
protected:
    TransformTest() : world("TransformTest"), em(world.GetEntityManager()), sm(world.GetSystemManager()) {}

    ecs::World          world;
    ecs::EntityManager& em;
    ecs::SystemManager& sm;
};

class RelationShipSystemTest : public TransformTest {
public:
    void SetUp() override {
        sm.Register<RelationShipSystem>();
    }
};

TEST_F(RelationShipSystemTest, UpdateChildren) {
    auto entities = em.CreateMany<RelationShip>(3);

    entities[1].GetComponent<RelationShip>().parent = entities[0];  // set parent of entities[1] to entities[0]
    entities[2].GetComponent<RelationShip>().parent = entities[1];  // set parent of entities[2] to entities[1]

    world.Update();

    EXPECT_EQ(entities[1].GetComponent<RelationShip>().parent, entities[0])
        << "the parent of entities[1] should be entities[0] after update";

    EXPECT_EQ(entities[2].GetComponent<RelationShip>().parent, entities[1])
        << "the parent of entities[2] should be entities[1] after update";

    ASSERT_TRUE(entities[0].GetComponent<RelationShip>().GetChildren().contains(entities[1]))
        << "entities[1] should be in the children of entities[0] after update";

    ASSERT_TRUE(entities[1].GetComponent<RelationShip>().GetChildren().contains(entities[2]))
        << "entities[2] should be in the children of entities[1] after update";
}

TEST_F(RelationShipSystemTest, AutoAttachChildren) {
    auto entities = em.CreateMany<RelationShip>(2);

    entities[1].GetComponent<RelationShip>().parent = entities[0];  // set parent of entities[1] to entity_

    world.Update();

    ASSERT_TRUE(entities[0].HasComponent<RelationShip>())
        << "entities[0] should be attached with Children component after update";

    EXPECT_TRUE(entities[0].GetComponent<RelationShip>().GetChildren().contains(entities[1]))
        << "entities[1] should be in the children of entities[0] after update";
}

class LocalToWorldSystemTest : public TransformTest {
public:
    void SetUp() override {
        sm.Register<RelationShipSystem>();
        sm.Register<TransformSystem>();
    }
};

TEST_F(LocalToWorldSystemTest, UpdateLocalMatrix) {
    auto individual_entity = em.Create<Transform>();  // Individual entity with transform
    {
        auto& individual_transform    = individual_entity.GetComponent<Transform>();
        individual_transform.position = math::vec3f{1.0f, 0.0f, 0.0f};
        individual_transform.rotation = math::quatf::identity();
        individual_transform.scale    = math::vec3f{1.0f, 1.0f, 1.0f};
    }

    world.Update();

    EXPECT_EQ(individual_entity.GetComponent<Transform>().world_matrix, math::translate(math::vec3f{1.0f, 0.0f, 0.0f}));
}

TEST_F(LocalToWorldSystemTest, UpdateHierarchy) {
    auto  root_entity       = em.Create<Transform, RelationShip>();  // Root entity with transform and children
    auto& root_transform    = root_entity.GetComponent<Transform>();
    root_transform.position = math::vec3f{2.0f, 0.0f, 0.0f};
    root_transform.rotation = math::quatf::identity();
    root_transform.scale    = math::vec3f{1.0f, 1.0f, 1.0f};

    auto  child_entity       = em.Create<Transform, RelationShip>();  // Child entity with transform and a parent
    auto& child_transform    = child_entity.GetComponent<Transform>();
    child_transform.position = math::vec3f{3.0f, 0.0f, 0.0f};
    child_transform.rotation = math::quatf::identity();
    child_transform.scale    = math::vec3f{1.0f, 1.0f, 1.0f};

    child_entity.GetComponent<RelationShip>().parent = root_entity;  // Set parent of child_entity to root_entity

    world.Update();

    EXPECT_EQ(root_entity.GetComponent<Transform>().world_matrix, math::translate(math::vec3f{2.0f, 0.0f, 0.0f}));
    EXPECT_EQ(child_entity.GetComponent<Transform>().world_matrix, math::translate(math::vec3f{5.0f, 0.0f, 0.0f}));
}

int main(int argc, char** argv) {
    spdlog::set_level(spdlog::level::trace);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}