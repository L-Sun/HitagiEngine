#pragma once
#include <hitagi/resource/material.hpp>
#include <hitagi/resource/material_instance.hpp>

namespace hitagi::resource {
class MaterialParser {
public:
    std::shared_ptr<Material> Parse(const core::Buffer& buffer);
};
}  // namespace hitagi::resource