#pragma once
#include <hitagi/asset/material.hpp>

namespace hitagi::asset {
class MaterialParser {
public:
    virtual ~MaterialParser() = default;
    MaterialParser(std::shared_ptr<spdlog::logger> logger = nullptr) : m_Logger(std::move(logger)) {}

    virtual std::shared_ptr<Material> Parse(const core::Buffer& buffer) = 0;

protected:
    std::shared_ptr<spdlog::logger> m_Logger;
};

class MaterialJSONParser : public MaterialParser {
public:
    using MaterialParser::MaterialParser;

    std::shared_ptr<Material> Parse(const core::Buffer& buffer) final;
};
}  // namespace hitagi::asset