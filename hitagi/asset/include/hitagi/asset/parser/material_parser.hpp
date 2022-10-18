#pragma once
#include <hitagi/asset/material.hpp>

namespace hitagi::asset {
class MaterialParser {
public:
    virtual ~MaterialParser() = default;
    MaterialParser(std::shared_ptr<spdlog::logger> logger = nullptr) : m_Logger(std::move(logger)) {}

    virtual auto Parse(const std::filesystem::path& path) -> std::shared_ptr<Material>;
    virtual auto Parse(const core::Buffer& buffer) -> std::shared_ptr<Material> = 0;

protected:
    std::shared_ptr<spdlog::logger> m_Logger;
};

class MaterialJSONParser : public MaterialParser {
public:
    using MaterialParser::MaterialParser;

    using MaterialParser::Parse;
    auto Parse(const core::Buffer& buffer) -> std::shared_ptr<Material> final;
};
}  // namespace hitagi::asset