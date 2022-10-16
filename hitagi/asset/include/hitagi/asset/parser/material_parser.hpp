#pragma once
#include <hitagi/asset/material.hpp>

namespace hitagi::asset {
class MaterialParser {
public:
    virtual ~MaterialParser() = default;

    virtual std::shared_ptr<Material> Parse(const core::Buffer& buffer) = 0;
};

class MaterialJSONParser : public MaterialParser {
public:
    std::shared_ptr<Material> Parse(const core::Buffer& buffer) final;
};
}  // namespace hitagi::asset