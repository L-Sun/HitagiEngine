#include <hitagi/asset/scene.hpp>

#include <tracy/Tracy.hpp>

namespace hitagi::asset {
struct FrameConstant {
    math::vec4f camera_pos;
    math::mat4f view;
    math::mat4f projection;
    math::mat4f proj_view;
    math::mat4f inv_view;
    math::mat4f inv_projection;
    math::mat4f inv_proj_view;
    math::vec4f light_position;
    math::vec4f light_pos_in_view;
    math::vec3f light_color;
    float       light_intensity;
};

struct InstanceConstant {
    math::mat4f model;
};

Scene::Scene(std::string_view name, xg::Guid guid)
    : Resource(name, guid),
      root(std::make_shared<SceneNode>(Transform{}, "name")),
      world(name) {
    world.RegisterSystem<TransformSystem>("TransformSystem");
}

void Scene::Update() {
    root->Update();
    world.Update();
}

}  // namespace hitagi::asset