export module render:scene;
import std;
import asset;
import ecs;
import math;

export namespace hitagi::render {
struct RenderView {
    math::vec3f camera_position = {};
    math::mat4f view            = math::mat4f::identity();
    math::mat4f projection      = math::mat4f::identity();
};

struct RenderDrawItem {
    std::shared_ptr<asset::Mesh>        mesh;
    math::mat4f                         transform = math::mat4f::identity();
    std::shared_ptr<asset::VertexArray> deformed_vertices;

    std::vector<std::shared_ptr<asset::Material>> material_overrides;
    auto                                          SubMeshes() const { return asset::ResolveSubMeshes(*mesh, material_overrides); }

    static auto FromEntity(ecs::Entity entity) -> RenderDrawItem {
        RenderDrawItem item{.mesh      = entity.Get<asset::MeshComponent>().mesh,
                            .transform = entity.Get<asset::Transform>().world_matrix};
        item.material_overrides = entity.Get<asset::MeshComponent>().material_overrides;
        if (entity.Has<asset::SkinnedMesh>()) {
            item.deformed_vertices = entity.Get<asset::SkinnedMesh>().deformed_vertices;
            if (!item.deformed_vertices) throw std::logic_error("Update the skinned scene before extracting draws");
        }
        return item;
    }
};

}  // namespace hitagi::render
