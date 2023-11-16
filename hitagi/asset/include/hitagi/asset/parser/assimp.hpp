#pragma once
#include <hitagi/asset/parser/scene_parser.hpp>
#include <hitagi/asset/parser/image_parser.hpp>
#include <utility>

namespace hitagi::asset {
class AssimpParser : public SceneParser {
public:
    AssimpParser(utils::EnumArray<std::shared_ptr<ImageParser>, ImageFormat> image_parsers,
                 std::shared_ptr<spdlog::logger>                             logger = nullptr)
        : SceneParser(std::move(logger)), m_ImageParsers(std::move(image_parsers)) {}

    auto Parse(const std::filesystem::path& path, const std::filesystem::path& resource_base_path = {}) -> std::shared_ptr<Scene> final;

private:
    utils::EnumArray<std::shared_ptr<ImageParser>, ImageFormat> m_ImageParsers;
};
}  // namespace hitagi::asset