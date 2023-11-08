#include <hitagi/asset/parser/gltf.hpp>
#include <hitagi/utils/buffer_stream.hpp>
#include <hitagi/core/timer.hpp>

#include <spdlog/spdlog.h>
#include <fx/gltf.h>

using namespace hitagi::math;

namespace hitagi::asset {
constexpr PrimitiveType convert_primitive(fx::gltf::Primitive::Mode mode) {
    switch (mode) {
        case fx::gltf::Primitive::Mode::Points:
            return PrimitiveType::PointList;
        case fx::gltf::Primitive::Mode::Lines:
            return PrimitiveType::LineList;
        case fx::gltf::Primitive::Mode::LineLoop:
            return PrimitiveType::Unkown;
        case fx::gltf::Primitive::Mode::LineStrip:
            return PrimitiveType::LineStrip;
        case fx::gltf::Primitive::Mode::Triangles:
            return PrimitiveType::TriangleList;
        case fx::gltf::Primitive::Mode::TriangleStrip:
            return PrimitiveType::TriangleStrip;
        case fx::gltf::Primitive::Mode::TriangleFan:
            return PrimitiveType::Unkown;
    }
}
constexpr VertexAttribute convert_attribute(std::string_view gltf_attribute) {
    if (gltf_attribute == "POSITION") {
        return VertexAttribute::Position;
    } else if (gltf_attribute == "NORMAL") {
        return VertexAttribute::Normal;
    } else if (gltf_attribute == "TANGENT") {
        return VertexAttribute::Tangent;
    } else if (gltf_attribute == "TEXCOORD_0") {
        return VertexAttribute::UV0;
    } else if (gltf_attribute == "TEXCOORD_1") {
        return VertexAttribute::UV1;
    } else if (gltf_attribute == "TEXCOORD_2") {
        return VertexAttribute::UV2;
    } else if (gltf_attribute == "TEXCOORD_3") {
        return VertexAttribute::UV3;
    } else if (gltf_attribute == "COLOR_0") {
        return VertexAttribute::Color0;
    } else if (gltf_attribute == "COLOR_1") {
        return VertexAttribute::Color1;
    } else if (gltf_attribute == "COLOR_2") {
        return VertexAttribute::Color2;
    } else if (gltf_attribute == "COLOR_3") {
        return VertexAttribute::Color3;
    } else if (gltf_attribute == "JOINTS_0") {
        return VertexAttribute::BlendIndex;
    } else if (gltf_attribute == "WEIGHTS_0") {
        return VertexAttribute::BlendWeight;
    } else {
        return VertexAttribute::Custom0;
    }
}

constexpr std::string_view convert_attribute(VertexAttribute attr) {
    switch (attr) {
        case VertexAttribute::Position:
            return "POSITION";
        case VertexAttribute::Normal:
            return "NORMAL";
        case VertexAttribute::Tangent:
            return "TANGENT";
        case VertexAttribute::UV0:
            return "TEXCOORD_0";
        case VertexAttribute::UV1:
            return "TEXCOORD_1";
        case VertexAttribute::UV2:
            return "TEXCOORD_2";
        case VertexAttribute::UV3:
            return "TEXCOORD_3";
        case VertexAttribute::Color0:
            return "COLOR_0";
        case VertexAttribute::Color1:
            return "COLOR_1";
        case VertexAttribute::Color2:
            return "COLOR_2";
        case VertexAttribute::Color3:
            return "COLOR_3";
        case VertexAttribute::BlendIndex:
            return "JOINTS_0";
        case VertexAttribute::BlendWeight:
            return "WEIGHTS_0";
        default:
            return "custom";
    }
}

template <typename T>
const T* get_componet_pointer(std::size_t index, std::size_t offset_in_buffer_view, std::size_t buffer_view_index, const fx::gltf::Document& doc) {
    const auto& buffer_view = doc.bufferViews.at(buffer_view_index);
    const auto& buffer      = doc.buffers.at(buffer_view.buffer);

    return reinterpret_cast<const T*>(buffer.data.data() + buffer_view.byteOffset + offset_in_buffer_view + index * buffer_view.byteStride);
}

constexpr std::size_t get_component_size(const fx::gltf::Accessor::ComponentType& type) {
    switch (type) {
        case fx::gltf::Accessor::ComponentType::None:
            return 0;
        case fx::gltf::Accessor::ComponentType::Byte:
        case fx::gltf::Accessor::ComponentType::UnsignedByte:
            return 8;
        case fx::gltf::Accessor::ComponentType::Short:
        case fx::gltf::Accessor::ComponentType::UnsignedShort:
            return 16;
        case fx::gltf::Accessor::ComponentType::UnsignedInt:
        case fx::gltf::Accessor::ComponentType::Float:
            return 32;
    }
}

constexpr std::size_t get_num_component(const fx::gltf::Accessor::Type type) {
    switch (type) {
        case fx::gltf::Accessor::Type::None:
            return 0;
        case fx::gltf::Accessor::Type::Scalar:
            return 1;
        case fx::gltf::Accessor::Type::Vec2:
            return 2;
        case fx::gltf::Accessor::Type::Vec3:
            return 3;
        case fx::gltf::Accessor::Type::Vec4:
            return 4;
        case fx::gltf::Accessor::Type::Mat2:
            return 4;
        case fx::gltf::Accessor::Type::Mat3:
            return 9;
        case fx::gltf::Accessor::Type::Mat4:
            return 16;
    }
}

template <typename ComponentType>
bool check_type(const fx::gltf::Accessor& accessor) {
    bool result = true;
    if constexpr (std::is_same_v<ComponentType, std::int8_t>) {
        result = result && accessor.componentType == fx::gltf::Accessor::ComponentType::Byte;
    } else if constexpr (std::is_same_v<ComponentType, std::uint8_t>) {
        result = result && accessor.componentType == fx::gltf::Accessor::ComponentType::UnsignedByte;
    } else if constexpr (std::is_same_v<ComponentType, std::int16_t>) {
        result = result && accessor.componentType == fx::gltf::Accessor::ComponentType::Short;
    } else if constexpr (std::is_same_v<ComponentType, std::uint16_t>) {
        result = result && accessor.componentType == fx::gltf::Accessor::ComponentType::UnsignedShort;
    } else if constexpr (std::is_same_v<ComponentType, std::uint32_t>) {
        result = result && accessor.componentType == fx::gltf::Accessor::ComponentType::UnsignedInt;
    } else if constexpr (std::is_same_v<ComponentType, float>) {
        result = result && accessor.componentType == fx::gltf::Accessor::ComponentType::Float;
    } else {
        result = result && false;
    }

    return result;
}

class Accessor {
public:
    Accessor(std::size_t accessor, const fx::gltf::Document& doc)
        : m_Accessor(doc.accessors.at(accessor)),
          m_Doc(doc),
          m_BufferView(doc.bufferViews.at(m_Accessor.bufferView)),
          m_Buffer(doc.buffers.at(m_BufferView.buffer)) {}

    inline std::size_t                       count() const noexcept { return m_Accessor.count; }
    inline fx::gltf::Accessor::Type          type() const noexcept { return m_Accessor.type; }
    inline fx::gltf::Accessor::ComponentType component_type() const noexcept { return m_Accessor.componentType; }

    template <typename T>
    const T get_scale(std::size_t i) {
        assert(check_type<T>(m_Accessor));
        assert(m_Accessor.type == fx::gltf::Accessor::Type::Scalar);

        return *get_value<T>(i);
    }

    template <typename T, unsigned N>
    Vector<T, N> get_vec(std::size_t i) {
        assert(check_type<T>(m_Accessor));

        if constexpr (N == 2) {
            assert(m_Accessor.type == fx::gltf::Accessor::Type::Vec2);
        } else if constexpr (N == 3) {
            assert(m_Accessor.type == fx::gltf::Accessor::Type::Vec3);
        } else if constexpr (N == 4) {
            assert(m_Accessor.type == fx::gltf::Accessor::Type::Vec4);
        } else {
            []<bool flag = false>() { static_assert(flag, "no match"); }
            ();
        }

        return {get_value<T>(i)};
    }

    template <typename T, unsigned N>
    Matrix<T, N> get_mat(std::size_t i) {
        assert(check_type<T>(m_Accessor));
        if constexpr (N == 2) {
            assert(m_Accessor.type == fx::gltf::Accessor::Type::Mat2);
        } else if constexpr (N == 3) {
            assert(m_Accessor.type == fx::gltf::Accessor::Type::Mat3);
        } else if constexpr (N == 4) {
            assert(m_Accessor.type == fx::gltf::Accessor::Type::Mat4);
        } else {
            []<bool flag = false>() { static_assert(flag, "no match"); }
            ();
        }

        // padding of component size, but no bytes
        // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#data-alignment
        constexpr auto get_padding = []() -> std::size_t {
            if constexpr ((N == 2 || N == 3) && (std::is_same_v<T, std::int8_t> || std::is_same_v<T, std::uint8_t>)) {
                return 2;
            } else if constexpr (N == 3 && (std::is_same_v<T, std::int16_t> || std::is_same_v<T, std::uint16_t>)) {
                return 1;
            }
        };

        constexpr std::size_t padding = get_padding();

        Matrix<T, N> result;
        auto         p = get_value<T>(i);

        for (std::size_t col = 0; col < N; col++) {
            for (std::size_t row = 0; row < N; row++) {
                result[row][col] = *p++;
            }
            p += padding;
        }
    }

protected:
    template <typename ComponentType>
    const ComponentType* get_value(std::size_t i) {
        if (auto sparse_value = get_sparse_value<ComponentType>(i); sparse_value) {
            return sparse_value;
        } else {
            return get_componet_pointer<ComponentType>(i, m_Accessor.byteOffset, m_Accessor.bufferView, m_Doc);
        }
    }

    template <typename ComponentType>
    const ComponentType* get_sparse_value(std::size_t i) {
        if (m_Accessor.sparse.empty()) return nullptr;

        std::optional<std::size_t> index;
        if (m_Accessor.sparse.indices.componentType == fx::gltf::Accessor::ComponentType::UnsignedShort) {
            index = get_sparse_values_index<std::uint16_t>(i);
        } else if (m_Accessor.sparse.indices.componentType == fx::gltf::Accessor::ComponentType::UnsignedInt) {
            index = get_sparse_values_index<std::uint32_t>(i);
        }

        if (!index.has_value()) return nullptr;

        return get_componet_pointer<ComponentType>(index.value(), m_Accessor.sparse.values.byteOffset, m_Accessor.sparse.values.bufferView, m_Doc);
    }

    template <typename T>
    std::optional<T> get_sparse_values_index(std::size_t i) {
        static_assert(std::is_same_v<T, std::uint16_t> || std::is_same_v<T, std::uint32_t>);
        if (m_Accessor.sparse.empty()) return std::nullopt;

        auto      p_indices = get_componet_pointer<T>(0, m_Accessor.sparse.indices.byteOffset, m_Accessor.bufferView, m_Doc);
        std::span indices{p_indices, static_cast<std::size_t>(m_Accessor.sparse.count)};

        auto iter = std::find(indices.begin(), indices.end(), i);
        if (iter != indices.end()) {
            return std::distance(indices.begin(), indices.end());
        }
        return std::nullopt;
    }

    const fx::gltf::Accessor&   m_Accessor;
    const fx::gltf::Document&   m_Doc;
    const fx::gltf::BufferView& m_BufferView;
    const fx::gltf::Buffer&     m_Buffer;
};

std::pmr::vector<Scene> GltfParser::Parse(const core::Buffer& buffer, const std::filesystem::path& root_path) {
    auto logger = spdlog::get("AssetManager");
    logger->info("Parser Scene...");

    core::Clock clock;
    clock.Start();

    utils::imemstream buffer_stream(buffer.GetData(), buffer.GetDataSize());

    fx::gltf::Document doc;
    try {
        doc = m_Binary ? fx::gltf::LoadFromBinary(buffer_stream, root_path) : fx::gltf::LoadFromText(buffer_stream, root_path);
        clock.Tick();
        logger->trace("Parse gltf cost {} ms.", std::chrono::duration_cast<std::chrono::milliseconds>(clock.DeltaTime()).count());
    } catch (fx::gltf::invalid_gltf_document ex) {
        logger->error(ex.what());
        return {};
    } catch (...) {
        logger->error("Cannot parse scene!");
        return {};
    }

    auto convert_mesh = [&](const fx::gltf::Mesh& gltf_mesh) -> std::shared_ptr<Mesh> {
        // Get vertex count and attribute
        std::size_t                vertices_count = 0;
        std::size_t                indices_count  = 0;
        VertexAttributeMask        attrs_mask;
        std::array<std::string, 2> custom_attributes;
        for (const auto& gltf_primitive : gltf_mesh.primitives) {
            std::size_t submesh_vertex_count = 0;

            for (const auto& [gltf_attribute, accessor_index] : gltf_primitive.attributes) {
                if (auto attr = convert_attribute(gltf_attribute); attr != VertexAttribute::Custom0) {
                    attrs_mask.set(magic_enum::enum_integer(attr));
                }
                // application custom attribute
                else if (gltf_attribute.front() == '_') {
                    if (!attrs_mask.test(magic_enum::enum_integer(VertexAttribute::Custom0))) {
                        custom_attributes[0] = gltf_attribute;
                        attrs_mask.set(magic_enum::enum_integer(VertexAttribute::Custom0));
                    } else if (!attrs_mask.test(magic_enum::enum_integer(VertexAttribute::Custom1))) {
                        custom_attributes[1] = gltf_attribute;
                        attrs_mask.set(magic_enum::enum_integer(VertexAttribute::Custom1));
                    } else {
                        logger->warn("Only two custom vertex attribute are supported for now! The gltf attribute will skip {}", gltf_attribute);
                    }
                } else {
                    logger->warn("Invalid gltf attribute {}. Please visit the following link for more infomation", gltf_attribute);
                    logger->warn("https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview:~:text=Application%2Dspecific%20attribute%20semantics%20MUST%20start%20with%20an%20underscore%2C%20e.g.%2C%20_TEMPERATURE.%20Application%2Dspecific%20attribute%20semantics%20MUST%20NOT%20use%20unsigned%20int%20component%20type.");
                }

                const auto& accessor = doc.accessors.at(accessor_index);

                if (submesh_vertex_count == 0) {
                    submesh_vertex_count = accessor.count;
                } else if (submesh_vertex_count != accessor.count) {
                    logger->warn("All the vertex count of attributes are no same! expect value: {}, current value: {}", submesh_vertex_count, accessor.count);
                    logger->warn("Your gltf file may be invalid. The Parser will skip this mesh {}. For more infomation, please visit the following link.", gltf_mesh.name);
                    logger->warn("https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview:~:text=All%20attribute%20accessors,accessors%27%20count.");
                    return nullptr;
                }
            }
            vertices_count += submesh_vertex_count;

            if (gltf_primitive.indices != -1) {
                const auto& accessor = doc.accessors.at(gltf_primitive.indices);
                indices_count += accessor.count;
            }
            // this submesh is not use indexed draw in gltf
            else {
                indices_count += vertices_count;
            }
        }

        auto mesh      = std::make_shared<Mesh>();
        mesh->name     = gltf_mesh.name;
        mesh->vertices = std::make_shared<VertexArray>(fmt::format("vertices-{}", gltf_mesh.name), vertices_count, attrs_mask);
        mesh->indices  = std::make_shared<IndexArray>(fmt::format("indices-{}", gltf_mesh.name), indices_count, IndexType::UINT32);

        std::size_t vertex_offset = 0, indices_offset = 0;
        for (const auto& gltf_primitive : gltf_mesh.primitives) {
            // Load position
            if (mesh->vertices->IsEnabled(VertexAttribute::Position)) {
                mesh->vertices->Modify<VertexAttribute::Position>([&](auto positions) {
                    Accessor position_accessor(gltf_primitive.attributes.at(std::string(convert_attribute(VertexAttribute::Position))), doc);
                    assert(positions.size() == position_accessor.count());
                    for (std::size_t i = 0; i < positions.size(); i++) {
                        positions[i] = position_accessor.get_vec<float, 3>(i);
                    }
                });
            }
            // Load normal
            if (mesh->vertices->IsEnabled(VertexAttribute::Normal)) {
                mesh->vertices->Modify<VertexAttribute::Normal>([&](auto normals) {
                    Accessor normal_accessor(gltf_primitive.attributes.at(std::string(convert_attribute(VertexAttribute::Normal))), doc);
                    assert(normals.size() == normal_accessor.count());
                    for (std::size_t i = 0; i < normals.size(); i++) {
                        normals[i] = normal_accessor.get_vec<float, 3>(i);
                    }
                });
            }

            // Load tangent
            if (mesh->vertices->IsEnabled(VertexAttribute::Tangent)) {
                mesh->vertices->Modify<VertexAttribute::Tangent>([&](auto tangents) {
                    Accessor tangent_accessor(gltf_primitive.attributes.at(std::string(convert_attribute(VertexAttribute::Tangent))), doc);
                    assert(tangents.size() == tangent_accessor.count());
                    for (std::size_t i = 0; i < tangents.size(); i++) {
                        tangents[i] = tangent_accessor.get_vec<float, 4>(i).xyz;
                    }
                });
            }
            // Load colors;
            {
                constexpr std::array channels{VertexAttribute::Color0, VertexAttribute::Color1, VertexAttribute::Color2, VertexAttribute::Color3};

                auto build_color = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
                    if (attrs_mask.test(I)) {
                        mesh->vertices->Modify<channels.at(I)>([&](auto colors) {
                            Accessor color_accessor(gltf_primitive.attributes.at(std::string(convert_attribute(channels.at(I)))), doc);
                            assert(colors.size() == color_accessor.count());

                            for (std::size_t i = 0; i < colors.size(); i++) {
                                if (color_accessor.type() == fx::gltf::Accessor::Type::Vec3) {
                                    colors[i] = vec4f(color_accessor.get_vec<float, 3>(i), 1.0f);
                                } else {
                                    colors[i] = color_accessor.get_vec<float, 4>(i);
                                }
                            }
                        });
                    }
                };
                [&]<std::size_t... I>(std::index_sequence<I...>) {
                    (build_color(std::integral_constant<std::size_t, I>{}), ...);
                }(std::make_index_sequence<channels.size()>{});
            }
            // Load UV
            {
                constexpr std::array channels{VertexAttribute::UV0, VertexAttribute::UV1, VertexAttribute::UV2, VertexAttribute::UV3};

                auto build_uv = [&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
                    if (attrs_mask.test(I)) {
                        mesh->vertices->Modify<channels.at(I)>([&](auto uvs) {
                            Accessor uv_accessor(gltf_primitive.attributes.at(std::string(convert_attribute(channels.at(I)))), doc);
                            assert(uvs.size() == uv_accessor.count());
                            for (std::size_t i = 0; i < uvs.size(); i++) {
                                switch (uv_accessor.component_type()) {
                                    case fx::gltf::Accessor::ComponentType::Float: {
                                        uvs[i] = uv_accessor.get_vec<float, 2>(i);
                                    } break;
                                    case fx::gltf::Accessor::ComponentType::Byte: {
                                        auto uv = uv_accessor.get_vec<std::int8_t, 2>(i);
                                    }
                                }
                            }
                        });
                    }
                };
                [&]<std::size_t... I>(std::index_sequence<I...>) {
                    (build_uv(std::integral_constant<std::size_t, I>{}), ...);
                }(std::make_index_sequence<channels.size()>{});
            }
        }

        return mesh;
    };

    std::function<void(const fx::gltf::Node&, const std::shared_ptr<SceneNode>&)>
        convert_node = [&](const fx::gltf::Node& gltf_node, const std::shared_ptr<SceneNode>& parent) -> void {
        // result node
        std::shared_ptr<SceneNode> node;

        // This node is mesh
        if (gltf_node.mesh != -1) {
            auto mesh_node    = std::make_shared<MeshNode>();
            mesh_node->object = std::make_shared<Mesh>();
        }
        // This node is camera
        else if (gltf_node.camera != -1) {
        }
        // This node is armute
        else if (gltf_node.skin != -1) {
        }
        // This node is empty node
        else {
        }

        node->Attach(parent);
        node->name                        = gltf_node.name;
        node->transform.local_translation = gltf_node.translation;
        node->transform.local_rotation    = gltf_node.rotation;
        node->transform.local_scaling     = gltf_node.scale;

        for (const auto& child_index : gltf_node.children) {
            convert_node(doc.nodes.at(child_index), node);
        }
    };

    for (const auto& gltf_scene : doc.scenes) {
        Scene scene(gltf_scene.name);

        for (auto node_index : gltf_scene.nodes) {
            convert_node(doc.nodes.at(node_index), scene.root);
        }
    }

    return {};
}

}  // namespace hitagi::asset