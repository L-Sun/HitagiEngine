#pragma once
#include <hitagi/resource/material.hpp>
#include <hitagi/resource/material_instance.hpp>

namespace hitagi::resource {
class MaterialParser {
public:
    virtual std::shared_ptr<Material> Parse(const core::Buffer& buffer) = 0;
};

class MaterialJSONParser : public MaterialParser {
public:
    std::shared_ptr<Material> Parse(const core::Buffer& buffer) final;
};
}  // namespace hitagi::resource