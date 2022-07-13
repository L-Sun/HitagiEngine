#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/resource/enums.hpp>
#include <hitagi/graphics/enums.hpp>
#include <hitagi/graphics/resource.hpp>
#include <hitagi/utils/private_build.hpp>

#include <bitset>

namespace hitagi::graphics {
class DriverAPI;

struct RootSignatureDetial {
public:
    struct Parameter {
        std::pmr::string   name;
        ShaderVariableType type;
        unsigned           register_index;
        unsigned           space;
        ShaderVisibility   visibility;
    };
    struct StaticSamplerDescription {
        std::pmr::string      name;
        resource::SamplerDesc desc;
        unsigned              register_index;
        unsigned              space;
        ShaderVisibility      visibility;
    };
    std::pmr::string                           name;
    std::pmr::vector<Parameter>                parameter_table;
    std::pmr::vector<StaticSamplerDescription> static_sampler_descs;
};

class RootSignature : public Resource, private RootSignatureDetial, public utils::enable_private_make_shared_build<RootSignature> {
public:
    class Builder : private RootSignatureDetial {
        friend class RootSignature;

    public:
        Builder& SetName(std::string_view name);

        Builder& Add(
            std::string_view   name,
            ShaderVariableType type,
            unsigned           register_index,
            unsigned           space,
            ShaderVisibility   visibility = ShaderVisibility::All);

        // TODO Sampler
        Builder& AddStaticSampler(
            std::string_view      name,
            resource::SamplerDesc desc,
            unsigned              register_index,
            unsigned              space,
            ShaderVisibility      visibility = ShaderVisibility::All);

        std::shared_ptr<RootSignature> Create(DriverAPI& driver);
    };

    inline auto& GetParametes() const noexcept { return parameter_table; }
    inline auto& GetStaticSamplerDescs() const noexcept { return static_sampler_descs; }

protected:
    RootSignature(const Builder&);
};

struct PipelineStateDetial {
    constexpr static auto num_vertex_slot = magic_enum::enum_count<resource::VertexAttribute>();

    std::pmr::string               name;
    core::Buffer                   vertex_shader;
    core::Buffer                   pixel_shader;
    bool                           front_counter_clockwise = true;
    std::shared_ptr<RootSignature> root_signature          = nullptr;
    Format                         render_format           = Format::R8G8B8A8_UNORM;
    Format                         depth_buffer_format     = Format::UNKNOWN;
    resource::PrimitiveType        primitive_type          = resource::PrimitiveType::TriangleList;
    BlendDescription               blend_state;
    RasterizerDescription          rasterizer_state;
};

class PipelineState : public Resource, public PipelineStateDetial, utils::enable_private_make_shared_build<PipelineState> {
public:
    class Builder : private PipelineStateDetial {
        friend class PipelineState;

    public:
        Builder& SetName(std::string_view name);
        Builder& SetVertexShader(core::Buffer vs);
        Builder& SetPixelShader(core::Buffer ps);
        Builder& SetRootSignautre(std::shared_ptr<RootSignature> sig);
        Builder& SetRenderFormat(Format format);
        Builder& SetDepthBufferFormat(Format format);
        Builder& SetPrimitiveType(resource::PrimitiveType type);
        Builder& SetRasterizerState(RasterizerDescription desc);
        Builder& SetBlendState(BlendDescription desc);

        std::shared_ptr<PipelineState> Build(DriverAPI& driver);
    };

protected:
    PipelineState(const Builder& builder);
};

}  // namespace hitagi::graphics