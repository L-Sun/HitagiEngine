#include <hitagi/resource/scene.hpp>

#include <algorithm>

namespace hitagi::resource {
Scene::Scene(std::string_view name)
    : name(name),
      root(std::make_shared<SceneNode>()) {
}

}  // namespace hitagi::resource
