#pragma once
#include "scene_object.hpp"
#include "mesh.hpp"

#include <map>
#include <vector>

namespace hitagi::asset {
class Geometry : public SceneObject {
protected:
    std::map<unsigned, std::vector<Mesh>> m_Meshes;

public:
    Geometry() = default;
    void SetVisibility(bool visible);

    void               AddMesh(Mesh mesh, size_t level = 0);
    inline const auto& GetMeshes(unsigned level = 0) const { return m_Meshes.at(level); };

    friend std::ostream& operator<<(std::ostream& out, const Geometry& obj);
};
}  // namespace hitagi::asset