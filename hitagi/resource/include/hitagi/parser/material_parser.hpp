#pragma once
#include <hitagi/resource/material.hpp>
#include <hitagi/resource/material_instance.hpp>

namespace hitagi::resource {
class MaterialParser {
public:
    std::shared_ptr<MaterialInstance> Parse(const core::Buffer& buffer, std::pmr::vector<std::shared_ptr<Material>>& materials);
};
}  // namespace hitagi::resource