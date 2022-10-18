#include <hitagi/asset/parser/material_parser.hpp>
#include <hitagi/core/file_io_manager.hpp>

#include <magic_enum.hpp>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <filesystem>

using namespace hitagi::math;

namespace hitagi::math {
template <typename T, unsigned N>
void to_json(nlohmann::json& j, const Vector<T, N>& vec) {
    j = vec.data;
}
template <typename T, unsigned N>
void from_json(const nlohmann::json& j, Vector<T, N>& p) {
    p.data = j;
}
template <typename T, unsigned N>
void to_json(nlohmann::json& j, const Matrix<T, N>& vec) {
    j = vec.data;
}
template <typename T, unsigned N>
void from_json(const nlohmann::json& j, Matrix<T, N>& p) {
    p.data = j;
}

}  // namespace hitagi::math

namespace hitagi::gfx {
NLOHMANN_JSON_SERIALIZE_ENUM(PrimitiveTopology,
                             {
                                 {PrimitiveTopology::PointList, "PointList"},
                                 {PrimitiveTopology::LineList, "LineList"},
                                 {PrimitiveTopology::LineStrip, "LineStrip"},
                                 {PrimitiveTopology::TriangleList, "TriangleList"},
                                 {PrimitiveTopology::TriangleStrip, "TriangleStrip"},
                                 {PrimitiveTopology::Unkown, nullptr},
                             })
}

namespace hitagi::asset {

auto MaterialParser::Parse(const std::filesystem::path& path) -> std::shared_ptr<Material> {
    return Parse(file_io_manager->SyncOpenAndReadBinary(path));
}

auto MaterialJSONParser::Parse(const core::Buffer& buffer) -> std::shared_ptr<Material> {
    auto logger = m_Logger ? m_Logger : spdlog::default_logger();

    if (buffer.Empty()) {
        logger->error("[MaterialPaser] Can not parse empty buffer!");
        return nullptr;
    }

    auto check_field = [logger](std::string_view name, const nlohmann::json& json) {
        if (json.contains(name)) return true;
        logger->error("[MaterialPaser] missing field: {}", name);
        return false;
    };

    nlohmann::json json;
    try {
        json = nlohmann::json::parse(buffer.Span<char>());
    } catch (std::exception ex) {
        logger->error(ex.what());
        return nullptr;
    }
    if (!(check_field("name", json) &&
          check_field("vertex_shader", json) &&
          check_field("pixel_shader", json) &&
          check_field("primitive", json))) return nullptr;

    gfx::RenderPipeline::Desc pipeline_desc = {
        .vs = {
            .name        = std::pmr::string{fmt::format("{}-vs", json["name"])},
            .type        = gfx::Shader::Type::Vertex,
            .entry       = "VSMain",
            .source_code = std::pmr::string{json["vertex_shader"]},
        },
        .ps = {
            .name        = std::pmr::string{fmt::format("{}-ps", json["name"])},
            .type        = gfx::Shader::Type::Pixel,
            .entry       = "PSMain",
            .source_code = std::pmr::string{json["pixel_shader"]},
        },
        .topology = json["primitive"].get<gfx::PrimitiveTopology>(),
        // TODO input layout
    };

    std::pmr::vector<Material::Parameter> parameters;

    if (json.contains("parameters")) {
        if (!json["parameters"].is_array()) {
            logger->error("parameters is not an array!");
            return nullptr;
        }

        for (auto param : json["parameters"]) {
            if (!(check_field("name", param) && check_field("type", param))) {
                return nullptr;
            }

            std::string_view    type = param["type"];
            Material::Parameter mat_param{.name = param["name"]};
            if (type == "float")
                mat_param.value = param.value("value", 0.0f);
            else if (type == "int32")
                mat_param.value = param.value("value", std::int32_t{0});
            else if (type == "uint32")
                mat_param.value = param.value("value", std::uint32_t{0});
            else if (type == "vec2i") {
                mat_param.value = param.value("value", vec2i{});
            } else if (type == "vec2u")
                mat_param.value = param.value("value", vec2u{});
            else if (type == "vec2f")
                mat_param.value = param.value("value", vec2f{});
            else if (type == "vec3i")
                mat_param.value = param.value("value", vec3i{});
            else if (type == "vec3u")
                mat_param.value = param.value("value", vec3u{});
            else if (type == "vec3f")
                mat_param.value = param.value("value", vec3f{});
            else if (type == "vec4i")
                mat_param.value = param.value("value", vec4i{});
            else if (type == "vec4u")
                mat_param.value = param.value("value", vec4u{});
            else if (type == "vec4f")
                mat_param.value = param.value("value", vec4f{});
            else if (type == "mat4f")
                mat_param.value = param.value("value", mat4f{});
            else if (type == "texture")
                mat_param.value = std::make_shared<Texture>(param.value("value", "default.tex"));
            else {
                logger->error("Unkown parameter type: {}", param["type"]);
                return nullptr;
            }
            parameters.emplace_back(std::move(mat_param));
        }
    }

    return Material::Create(std::move(pipeline_desc), std::move(parameters), json["name"]);
}

}  // namespace hitagi::asset