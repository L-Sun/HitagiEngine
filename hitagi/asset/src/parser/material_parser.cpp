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

    nlohmann::json json;
    try {
        json = nlohmann::json::parse(buffer.Span<char>());

        gfx::RenderPipeline::Desc pipeline_desc{
            .vs = {
                .name        = json.at("pipeline").at("vs"),
                .type        = gfx::ShaderType::Vertex,
                .entry       = "VSMain",
                .source_code = std::pmr::string{file_io_manager->SyncOpenAndReadBinary(json.at("pipeline").at("vs")).Str()},
            },
            .ps = {
                .name        = json.at("pipeline").at("ps"),
                .type        = gfx::ShaderType::Pixel,
                .entry       = "PSMain",
                .source_code = std::pmr::string{file_io_manager->SyncOpenAndReadBinary(json.at("pipeline").at("ps")).Str()},
            },
        };

        if (json.at("pipeline").contains("primitive")) {
            pipeline_desc.topology = json.at("pipeline")["primitive"].get<gfx::PrimitiveTopology>();
        }

        std::pmr::vector<Material::Parameter> parameters;
        if (json.contains("parameters")) {
            if (!json["parameters"].is_array()) {
                logger->error("parameters is not an array!");
                return nullptr;
            }

            for (auto param : json["parameters"]) {
                const std::string&  type = param.at("type");
                Material::Parameter mat_param{.name = param.at("name")};
                if (type == "float")
                    mat_param.value = param.value("default", 0.0f);
                else if (type == "int32")
                    mat_param.value = param.value("default", std::int32_t{0});
                else if (type == "uint32")
                    mat_param.value = param.value("default", std::uint32_t{0});
                else if (type == "vec2i") {
                    mat_param.value = param.value("default", vec2i{});
                } else if (type == "vec2u")
                    mat_param.value = param.value("default", vec2u{});
                else if (type == "vec2f")
                    mat_param.value = param.value("default", vec2f{});
                else if (type == "vec3i")
                    mat_param.value = param.value("default", vec3i{});
                else if (type == "vec3u")
                    mat_param.value = param.value("default", vec3u{});
                else if (type == "vec3f")
                    mat_param.value = param.value("default", vec3f{});
                else if (type == "vec4i")
                    mat_param.value = param.value("default", vec4i{});
                else if (type == "vec4u")
                    mat_param.value = param.value("default", vec4u{});
                else if (type == "vec4f")
                    mat_param.value = param.value("default", vec4f{});
                else if (type == "mat4f")
                    mat_param.value = param.value("default", mat4f{});
                else if (type == "texture") {
                    if (param.contains("default"))
                        mat_param.value = std::make_shared<Texture>(param["default"]);
                    else
                        mat_param.value = Texture::DefaultTexture();
                } else {
                    logger->error("Unkown parameter type: {}", param["type"]);
                    return nullptr;
                }
                parameters.emplace_back(std::move(mat_param));
            }
        }
        const std::string& name = json.at("name");
        return Material::Create(std::move(pipeline_desc), std::move(parameters), name);
    } catch (nlohmann::json::exception& ex) {
        logger->error(ex.what());
        return nullptr;
    }
}

}  // namespace hitagi::asset