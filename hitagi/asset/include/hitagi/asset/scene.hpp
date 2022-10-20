#pragma once
#include <hitagi/asset/transform.hpp>
#include <hitagi/asset/material.hpp>
#include <hitagi/asset/camera.hpp>
#include <hitagi/asset/light.hpp>
#include <hitagi/asset/mesh.hpp>
#include <hitagi/asset/armature.hpp>
#include <hitagi/asset/scene_node.hpp>
#include <hitagi/gfx/render_graph.hpp>

#include <unordered_map>

namespace hitagi::asset {
class Scene : public Resource {
public:
    using Resource::Resource;

    struct RenderPass {
        // output
        gfx::ResourceHandle render_target;
        gfx::ResourceHandle depth_buffer;
        gfx::ResourceHandle frame_constant;
        gfx::ResourceHandle instance_constant;

        std::pmr::unordered_map<Material*, gfx::ResourceHandle> material_constants;
    };
    auto Render(gfx::RenderGraph& render_graph, gfx::ViewPort viewport, const std::shared_ptr<CameraNode>& camera = nullptr) -> RenderPass;

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