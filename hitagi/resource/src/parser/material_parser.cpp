#include <hitagi/parser/material_parser.hpp>

#include <spdlog/spdlog.h>
#include <yaml-cpp/yaml.h>

#include <filesystem>

using namespace hitagi::math;

namespace YAML {
template <typename T, unsigned N>
struct convert<Vector<T, N>> {
    static Node encode(const Vector<T, N>& rhs) {
        Node node;
        for (unsigned i = 0; i < N; i++) {
            node.push_back(rhs[i]);
        }
        return node;
    }

    static bool decode(const Node& node, Vector<T, N>& rhs) {
        if (!node.IsSequence() || node.size() != N) {
            return false;
        }
        for (unsigned i = 0; i < N; i++) {
            rhs[i] = node[i].as<T>();
        }
        return true;
    }
};

template <>
struct convert<hitagi::resource::PrimitiveType> {
    static Node encode(const hitagi::resource::PrimitiveType primitive) {
        Node node;
        node = std::string(magic_enum::enum_name(primitive));
        return node;
    }

    static bool decode(const Node& node, hitagi::resource::PrimitiveType& rhs) {
        if (!node.IsScalar()) {
            return false;
        }
        auto result = magic_enum::enum_cast<hitagi::resource::PrimitiveType>(node.as<std::string>());
        if (result.has_value()) {
            rhs = result.value();
            return true;
        } else {
            return false;
        }
    }
};
template <>
struct convert<std::filesystem::path> {
    static Node encode(const std::filesystem::path& rhs) {
        Node node;
        node = rhs.string();
        return node;
    }

    static bool decode(const Node& node, std::filesystem::path& rhs) {
        if (!node.IsScalar()) {
            return false;
        }
        rhs = node.as<std::string>();
        return true;
    }
};

}  // namespace YAML

namespace hitagi::resource {
std::shared_ptr<MaterialInstance> MaterialParser::Parse(const core::Buffer& buffer, std::pmr::vector<std::shared_ptr<Material>>& materials) {
    if (buffer.Empty()) return nullptr;

    auto logger = spdlog::get("AssetManager");

    YAML::Node node;
    try {
        node = YAML::Load(reinterpret_cast<const char*>(buffer.GetData()));

        auto logger = spdlog::get("AssetManager");

        Material::Builder builder;
        builder
            .SetName(node["name"].as<std::string>())
            .SetPrimitive(node["primitive"].as<PrimitiveType>())
            .SetShader(node["shader"].as<std::filesystem::path>());

        for (auto vertex_attri : node["vertexs"]) {
            auto slot = magic_enum::enum_cast<VertexAttribute>(vertex_attri.as<std::string>());
            if (!slot.has_value()) {
                throw std::invalid_argument(fmt::format("Unkown vertex attributes {}", vertex_attri.as<std::string>()));
            }
        }

        for (auto param : node["parameters"]) {
            std::pmr::string name{param["name"].as<std::string>()};
            std::pmr::string type{param["type"].as<std::string>()};

            if (type == "float")
                builder.AppendParameterInfo(name, param["value"].as<float>());
            else if (type == "int32")
                builder.AppendParameterInfo(name, param["value"].as<std::int32_t>());
            else if (type == "uint32")
                builder.AppendParameterInfo(name, param["value"].as<std::uint32_t>());
            else if (type == "vec2i") {
                builder.AppendParameterInfo(name, param["value"].as<vec2i>());
            } else if (type == "vec2u")
                builder.AppendParameterInfo(name, param["value"].as<vec2u>());
            else if (type == "vec2f")
                builder.AppendParameterInfo(name, param["value"].as<vec2f>());
            else if (type == "vec3i")
                builder.AppendParameterInfo(name, param["value"].as<vec3i>());
            else if (type == "vec3u")
                builder.AppendParameterInfo(name, param["value"].as<vec3u>());
            else if (type == "vec3f")
                builder.AppendParameterInfo(name, param["value"].as<vec3f>());
            else if (type == "vec4i")
                builder.AppendParameterInfo(name, param["value"].as<vec4i>());
            else if (type == "vec4u")
                builder.AppendParameterInfo(name, param["value"].as<vec4u>());
            else if (type == "vec4f")
                builder.AppendParameterInfo(name, param["value"].as<vec4f>());
            else if (type == "mat4f")
                builder.AppendParameterInfo(name, param["value"].as<vec4f>());
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
                logger->error("Unkown parameter type: {}", type);
                throw std::invalid_argument(fmt::format("Unkown parameter type: {}", type));
            }
        }

        for (auto texture : node["textures"]) {
            builder.AppendTextureName(texture["name"].as<std::string>(), texture["path"].as<std::filesystem::path>());
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

    } catch (YAML::ParserException ex) {
        logger->error(ex.what());
        return nullptr;
    } catch (std::exception ex) {
        logger->error(ex.what());
        return nullptr;
    }

    return nullptr;
}

}  // namespace hitagi::resource