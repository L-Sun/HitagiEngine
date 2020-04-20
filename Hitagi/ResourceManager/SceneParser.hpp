#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "Scene.hpp"
#include "Buffer.hpp"

namespace Hitagi::Resource {
class SceneParser {
public:
    virtual std::unique_ptr<Scene> Parse(const Core::Buffer& buf) = 0;

protected:
    SceneParser(std::shared_ptr<spdlog::logger> logger) : m_Logger(logger) {}
    std::shared_ptr<spdlog::logger> m_Logger;
};
}  // namespace Hitagi::Resource
