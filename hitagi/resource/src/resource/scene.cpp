#include <algorithm>
#include <hitagi/resource/scene.hpp>
#include "hitagi/resource/renderable.hpp"

namespace hitagi::resource {

// TODO sort
std::pmr::vector<Renderable> Scene::GetRenderable() {
    std::pmr::vector<Renderable> result;

    for (const auto& geometry : geometries) {
        if (geometry.visiable) {
            for (const auto& mesh : geometry.meshes) {
                Renderable item;
                item.mesh      = mesh;
                item.material  = mesh.material->GetMaterial().lock();
                item.transform = geometry.transform;
                result.emplace_back(std::move(item));
            }
        }
    }

    std::sort(result.begin(), result.end(), [](const Renderable& a, const Renderable& b) {
        return a.material <= b.material;
    });

    return result;
}
}  // namespace hitagi::resource
