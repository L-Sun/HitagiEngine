#include <hitagi/asset/scene.hpp>

#include <tracy/Tracy.hpp>

namespace hitagi::asset {

Scene::Scene(std::string_view name)
    : Resource(Type::Scene, name),
      root(std::make_shared<SceneNode>(Transform{}, "name")),
      world(name) {
}

void Scene::Update() {
    root->Update();
    world.Update();
}

}  // namespace hitagi::asset