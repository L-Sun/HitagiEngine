#include "Geometry.hpp"

namespace hitagi::asset {
void Geometry::AddMesh(Mesh mesh, size_t level) {
    m_Meshes[level].emplace_back(std::move(mesh));
}

std::ostream& operator<<(std::ostream& out, const Geometry& obj) {
    out << static_cast<const SceneObject&>(obj) << std::endl;

    for (auto&& [level, meshes] : obj.m_Meshes) {
        out << fmt::format("Level {}: \n", level);
        for (size_t i = 0; i < meshes.size(); i++) {
            out << fmt::format("Mesh[{}, {}]: \n", level, i);
            out << meshes.at(i);
        }
    }
    return out;
}

}  // namespace hitagi::asset