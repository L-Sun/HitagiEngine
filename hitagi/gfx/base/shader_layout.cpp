export module gfx.base:shader_layout;
import std;
import math;
import :bindless;

export namespace hitagi::gfx {

// Describes the raw, 32-bit storage-buffer ABI used by bindless.hlsl.
// Fields are typed from their CPU members; resource handles additionally name
// the shader wrapper. This is not a constant-buffer packing description.
struct ShaderField {
    std::string_view name;
    std::string_view hlsl_type;
    std::size_t      offset;
    std::size_t      size;
};

template <typename T>
constexpr auto ShaderFieldType() -> std::string_view {
    if constexpr (std::is_same_v<T, float>)
        return "float";
    else if constexpr (std::is_same_v<T, std::int32_t>)
        return "int";
    else if constexpr (std::is_same_v<T, std::uint32_t>)
        return "uint";
    else if constexpr (std::is_same_v<T, math::vec2f>)
        return "float2";
    else if constexpr (std::is_same_v<T, math::vec3f>)
        return "float3";
    else if constexpr (std::is_same_v<T, math::vec4f> || std::is_same_v<T, math::Color>)
        return "float4";
    else if constexpr (std::is_same_v<T, math::vec2i>)
        return "int2";
    else if constexpr (std::is_same_v<T, math::vec3i>)
        return "int3";
    else if constexpr (std::is_same_v<T, math::vec4i>)
        return "int4";
    else if constexpr (std::is_same_v<T, math::vec2u>)
        return "uint2";
    else if constexpr (std::is_same_v<T, math::vec3u>)
        return "uint3";
    else if constexpr (std::is_same_v<T, math::vec4u>)
        return "uint4";
    else if constexpr (std::is_same_v<T, math::mat4f>)
        return "row_major float4x4";
    else if constexpr (std::is_same_v<T, BindlessHandle>)
        return "hitagi::BindlessHandle";
    else
        static_assert(sizeof(T) == 0, "Unsupported shader storage field type");
}

template <typename T>
constexpr auto MakeShaderField(std::string_view name, std::size_t offset) -> ShaderField {
    static_assert(sizeof(T) % 4 == 0);
    return {name, ShaderFieldType<T>(), offset, sizeof(T)};
}

constexpr auto MakeShaderResourceField(std::string_view name, std::size_t offset, BindlessHandleType type) -> ShaderField {
    const std::string_view wrapper = type == BindlessHandleType::Buffer    ? "hitagi::SimpleBuffer"
                                     : type == BindlessHandleType::Texture ? "hitagi::Texture"
                                     : type == BindlessHandleType::Sampler ? "hitagi::Sampler"
                                                                           : "hitagi::BindlessHandle";
    return {name, wrapper, offset, sizeof(BindlessHandle)};
}

inline auto GenerateShaderStruct(std::string_view name, std::span<const ShaderField> fields, std::size_t size) -> std::pmr::string {
    std::pmr::string                          source(std::format("struct {} {{\n", name));
    std::size_t                               offset  = 0;
    std::size_t                               padding = 0;
    std::pmr::unordered_set<std::string_view> names;
    for (const ShaderField& field : fields) {
        if (field.offset < offset || field.offset % 4 != 0 || field.size % 4 != 0 ||
            field.offset > size || field.size > size - field.offset ||
            field.name.empty() || !names.insert(field.name).second) {
            throw std::invalid_argument("Invalid CPU/shader storage layout");
        }
        while (offset < field.offset) {
            source += std::format("    uint _hitagi_padding_{};\n", padding++);
            offset += 4;
        }
        source += std::format("    {} {};\n", field.hlsl_type, field.name);
        offset += field.size;
    }
    if (size < offset || size % 4 != 0) throw std::invalid_argument("Invalid shader storage size");
    while (offset < size) {
        source += std::format("    uint _hitagi_padding_{};\n", padding++);
        offset += 4;
    }
    source += "};\n\n";
    return source;
}

}  // namespace hitagi::gfx
