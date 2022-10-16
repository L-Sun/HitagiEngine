#pragma once
#include <hitagi/asset/transform.hpp>
#include <hitagi/asset/material.hpp>
#include <hitagi/asset/camera.hpp>
#include <hitagi/asset/light.hpp>
#include <hitagi/asset/mesh.hpp>
#include <hitagi/asset/armature.hpp>
#include <hitagi/asset/scene_node.hpp>

#include <crossguid/guid.hpp>

namespace hitagi::asset {
struct Scene {
    template <typename T>
    using SharedPtrVector = std::pmr::vector<std::shared_ptr<T>>;

    Scene(std::string_view name = "");

    std::pmr::string name;

    std::shared_ptr<SceneNode>    root;
    SharedPtrVector<MeshNode>     instance_nodes;
    SharedPtrVector<CameraNode>   camera_nodes;
    SharedPtrVector<LightNode>    light_nodes;
    SharedPtrVector<ArmatureNode> armature_nodes;

    std::shared_ptr<CameraNode> curr_camera;
};

}  // namespace hitagi::asset