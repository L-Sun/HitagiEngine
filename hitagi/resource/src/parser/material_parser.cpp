#include <hitagi/parser/material_parser.hpp>

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

namespace hitagi::resource {

NLOHMANN_JSON_SERIALIZE_ENUM(PrimitiveType,
                             {
                                 {PrimitiveType::PointList, "PointList"},
                                 {PrimitiveType::LineList, "LineList"},
                                 {PrimitiveType::LineStrip, "LineStrip"},
                                 {PrimitiveType::TriangleList, "TriangleList"},
                                 {PrimitiveType::TriangleStrip, "TriangleStrip"},
                                 {PrimitiveType::LineListAdjacency, "LineListAdjacency"},
                                 {PrimitiveType::LineStripAdjacency, "LineStripAdjacency"},
                                 {PrimitiveType::TriangleListAdjacency, "TriangleListAdjacency"},
                                 {PrimitiveType::TriangleStripAdjacency, "TriangleStripAdjacency"},
                                 {PrimitiveType::Unkown, nullptr},
                             })

std::shared_ptr<Material> MaterialJSONParser::Parse(const core::Buffer& buffer) {
    if (buffer.Empty()) return nullptr;

    auto logger = spdlog::get("AssetManager");

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

    Material::Builder builder;
    builder
        .SetName(json["name"])
        .SetVertexShader(json["vertex_shader"])
        .SetPixelShader(json["pixel_shader"])
        .SetPrimitive(json["primitive"]);

    if (json.contains("parameters")) {
        if (!json["parameters"].is_array()) {
            logger->error("parameters is not an array!");
            return nullptr;
        }

        for (auto param : json["parameters"]) {
            if (!(check_field("name", param) && check_field("type", param))) {
                return nullptr;
            }

            std::string_view type = param["type"], name = param["name"];

            if (type == "float")
                builder.AppendParameterInfo(name, param.value("default", 0.0f));
            else if (type == "int32")
                builder.AppendParameterInfo(name, param.value("default", std::int32_t{0}));
            else if (type == "uint32")
                builder.AppendParameterInfo(name, param.value("default", std::uint32_t{0}));
            else if (type == "vec2i") {
                builder.AppendParameterInfo(name, param.value("default", vec2i{}));
            } else if (type == "vec2u")
                builder.AppendParameterInfo(name, param.value("default", vec2u{}));
            else if (type == "vec2f")
                builder.AppendParameterInfo(name, param.value("default", vec2f{}));
            else if (type == "vec3i")
                builder.AppendParameterInfo(name, param.value("default", vec3i{}));
            else if (type == "vec3u")
                builder.AppendParameterInfo(name, param.value("default", vec3u{}));
            else if (type == "vec3f")
                builder.AppendParameterInfo(name, param.value("default", vec3f{}));
            else if (type == "vec4i")
                builder.AppendParameterInfo(name, param.value("default", vec4i{}));
            else if (type == "vec4u")
                builder.AppendParameterInfo(name, param.value("default", vec4u{}));
            else if (type == "vec4f")
                builder.AppendParameterInfo(name, param.value("default", vec4f{}));
            else if (type == "mat4f")
                builder.AppendParameterInfo(name, param.value("default", mat4f{}));
            else if (type == "texture")
                builder.AppendParameterInfo<std::shared_ptr<Texture>>(name, nullptr);
            else {
                logger->error("Unkown parameter type: {}", param["type"]);
                return nullptr;
            }
        }
    }

    return builder.Build();
}

}  // namespace hitagi::resource