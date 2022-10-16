#include <hitagi/asset/scene.hpp>

#include <algorithm>

namespace hitagi::asset {
Scene::Scene(std::string_view name)
    : name(name),
      root(std::make_shared<SceneNode>()) {
}

}  // namespace hitagi::asset
