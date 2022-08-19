#pragma once
#include <hitagi/resource/transform.hpp>
#include <hitagi/resource/material.hpp>
#include <hitagi/resource/camera.hpp>
#include <hitagi/resource/light.hpp>
#include <hitagi/resource/mesh.hpp>
#include <hitagi/resource/armature.hpp>
#include <hitagi/resource/scene_node.hpp>

#include <crossguid/guid.hpp>

namespace hitagi::resource {
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

    // TODO move this resource to asset manager so that we can reuse these in different scene
    SharedPtrVector<Mesh>     meshes;
    SharedPtrVector<Camera>   cameras;
    SharedPtrVector<Light>    lights;
    SharedPtrVector<Armature> armatures;
    SharedPtrVector<Material> materials;
};

}  // namespace hitagi::resource