#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "Image.hpp"
#include "Buffer.hpp"

namespace Hitagi::Resource {
class ImageParser {
public:
    virtual Image Parse(const Core::Buffer& buf) = 0;

protected:
    ImageParser(std::shared_ptr<spdlog::logger> logger) : m_Logger(logger) {}
    std::shared_ptr<spdlog::logger> m_Logger;
};
}  // namespace Hitagi::Resource
