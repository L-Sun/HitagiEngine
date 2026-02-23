module;

export module asset:mesh_factory;
import std;
import :mesh;
import math;

export namespace hitagi::asset {
class MeshFactory {
public:
    static auto Line(const math::vec3f& from, const math::vec3f& to) -> std::shared_ptr<Mesh>;
    static auto BoxWireframe(const math::vec3f& bb_min, const math::vec3f& bb_max) -> std::shared_ptr<Mesh>;
    static auto Cube() -> std::shared_ptr<Mesh>;
};
}  // namespace hitagi::asset
