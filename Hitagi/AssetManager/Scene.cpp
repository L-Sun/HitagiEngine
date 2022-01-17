#include "Scene.hpp"

namespace Hitagi::Asset {

void Scene::AddSkeleton(std::shared_ptr<BoneNode> skeleton) {
    // TODO
    scene_graph->AppendChild(skeleton);
    std::function<void(std::shared_ptr<BoneNode>)> recusive = [&](std::shared_ptr<BoneNode> bone) {
        bone_nodes.emplace_back(bone);
        for (auto&& child : bone->GetChildren()) {
            recusive(std::static_pointer_cast<BoneNode>(child));
        }
    };
    recusive(skeleton);
}

void Scene::AddAnimation(std::shared_ptr<Animation> animation) {
    if (animation) animations.emplace_back(animation);
}

std::shared_ptr<CameraNode> Scene::GetFirstCameraNode() const {
    return (camera_nodes.empty() ? nullptr : camera_nodes.front());
}
std::shared_ptr<LightNode> Scene::GetFirstLightNode() const {
    return (light_nodes.empty() ? nullptr : light_nodes.front());
}

void Scene::LoadResource() {
    for (auto& material : materials) {
        material->LoadTextures();
    }
}
}  // namespace Hitagi::Asset