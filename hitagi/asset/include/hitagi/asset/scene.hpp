#pragma once
#include <hitagi/asset/transform.hpp>
#include <hitagi/asset/material.hpp>
#include <hitagi/asset/camera.hpp>
#include <hitagi/asset/light.hpp>
#include <hitagi/asset/mesh.hpp>
#include <hitagi/asset/armature.hpp>
#include <hitagi/asset/scene_node.hpp>

#include <hitagi/gfx/render_graph.hpp>

namespace hitagi::asset {
class Scene : public Resource {
public:
    using Resource::Resource;

    template <typename T>
    using SharedPtrVector = std::pmr::vector<std::shared_ptr<T>>;

    std::shared_ptr<SceneNode>    root;
    SharedPtrVector<MeshNode>     instance_nodes;
    SharedPtrVector<CameraNode>   camera_nodes;
    SharedPtrVector<LightNode>    light_nodes;
    SharedPtrVector<ArmatureNode> armature_nodes;

    std::shared_ptr<CameraNode> curr_camera;
};

}  // namespace hitagi::asset