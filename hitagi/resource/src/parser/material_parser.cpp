#include <hitagi/parser/material_parser.hpp>

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

std::shared_ptr<MaterialInstance> MaterialParser::Parse(const core::Buffer& buffer, std::pmr::vector<std::shared_ptr<Material>>& materials) {
    if (buffer.Empty()) return nullptr;

    auto logger = spdlog::get("AssetManager");

    nlohmann::json json;
    try {
        json = nlohmann::json::parse(buffer.Span<char>());
    } catch (std::exception ex) {
        logger->error(ex.what());
        return nullptr;
    }

    Material::Builder builder;
    builder
        .SetName(json["name"])
        .SetShader(json["shader"]);

    if (auto primitive = magic_enum::enum_cast<PrimitiveType>(json["primitive"].get<std::string_view>()); primitive.has_value()) {
        builder.SetPrimitive(*primitive);
    } else {
        logger->warn("Unkown primitive type: {}", json["name"]);
    }

    for (const auto& vertex_attri : json["vertex_attributes"]) {
        auto slot = magic_enum::enum_cast<VertexAttribute>(vertex_attri.get<std::string>());
        if (!slot.has_value()) {
            logger->error("Unkown vertex attributes {}", vertex_attri);
            return nullptr;
        }
        builder.EnableSlot(slot.value());
    }

    if (json.contains("parameters")) {
        if (!json["parameters"].is_array()) {
            logger->error("parameters field must be a type of sequence");
            return nullptr;
        }

        for (auto param : json["parameters"]) {
            std::string_view type = param["type"], name = param["name"];

            if (type == "float")
                builder.AppendParameterInfo<float>(name, param["value"]);
            else if (type == "int32")
                builder.AppendParameterInfo<std::int32_t>(name, param["value"]);
            else if (type == "uint32")
                builder.AppendParameterInfo<std::uint32_t>(name, param["value"]);
            else if (type == "vec2i") {
                builder.AppendParameterInfo<vec2i>(name, param["value"]);
            } else if (type == "vec2u")
                builder.AppendParameterInfo<vec2u>(name, param["value"]);
            else if (type == "vec2f")
                builder.AppendParameterInfo<vec2f>(name, param["value"]);
            else if (type == "vec3i")
                builder.AppendParameterInfo<vec3i>(name, param["value"]);
            else if (type == "vec3u")
                builder.AppendParameterInfo<vec3u>(name, param["value"]);
            else if (type == "vec3f")
                builder.AppendParameterInfo<vec3f>(name, param["value"]);
            else if (type == "vec4i")
                builder.AppendParameterInfo<vec4i>(name, param["value"]);
            else if (type == "vec4u")
                builder.AppendParameterInfo<vec4u>(name, param["value"]);
            else if (type == "vec4f")
                builder.AppendParameterInfo<vec4f>(name, param["value"]);
            else if (type == "mat4f")
                builder.AppendParameterInfo<mat4f>(name, param["value"]);
            // else if (type == "float[]")
            //     builder.AppendParameterArrayInfo<float>(name, param["value"].size());
            // else if (type == "int32[]")
            //     builder.AppendParameterArrayInfo<std::int32_t>(name, param["value"].size());
            // else if (type == "uint32[]")
            //     builder.AppendParameterArrayInfo<std::uint32_t>(name, param["value"].size());
            // else if (type == "vec2i[]")
            //     builder.AppendParameterArrayInfo<vec2i>(name, param["value"].size());
            // else if (type == "vec2u[]")
            //     builder.AppendParameterArrayInfo<vec2u>(name, param["value"].size());
            // else if (type == "vec2f[]")
            //     builder.AppendParameterArrayInfo<vec2f>(name, param["value"].size());
            // else if (type == "vec3i[]")
            //     builder.AppendParameterArrayInfo<vec3i>(name, param["value"].size());
            // else if (type == "vec3u[]")
            //     builder.AppendParameterArrayInfo<vec3u>(name, param["value"].size());
            // else if (type == "vec3f[]")
            //     builder.AppendParameterArrayInfo<vec3f>(name, param["value"].size());
            // else if (type == "vec4i[]")
            //     builder.AppendParameterArrayInfo<vec4i>(name, param["value"].size());
            // else if (type == "vec4u[]")
            //     builder.AppendParameterArrayInfo<vec4u>(name, param["value"].size());
            // else if (type == "vec4f[]")
            //     builder.AppendParameterArrayInfo<vec4f>(name, param["value"].size());
            // else if (type == "mat4f[]")
            //     builder.AppendParameterArrayInfo<vec4f>(name, param["value"].size());
            else {
                logger->error("Unkown parameter type: {}", param["type"]);
                return nullptr;
            }
        }
    }

    for (auto texture : json["textures"]) {
        builder.AppendTextureName(texture["name"], texture["path"]);
    }

    auto material = builder.Build();
    auto iter     = std::find_if(materials.begin(), materials.end(), [material](const auto& m) {
        return *m == *material;
    });

    auto instance = material->CreateInstance();
    // A new material type
    if (iter == materials.end()) {
        materials.emplace_back(material);
    } else {
        instance->SetMaterial(*iter);
    }

    return instance;
}

}  // namespace hitagi::resource