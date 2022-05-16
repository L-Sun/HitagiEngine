#pragma once
#include <hitagi/core/buffer.hpp>
#include <hitagi/graphics/resource.hpp>
#include <hitagi/graphics/enums.hpp>
#include <hitagi/utils/private_build.hpp>

namespace hitagi::graphics {
class DriverAPI;

struct RootSignatureDetial {
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;
    struct Parameter {
        Parameter(allocator_type alloc = {}) : name(alloc) {}
        std::pmr::string   name;
        ShaderVariableType type;
        unsigned           register_index;
        unsigned           space;
        ShaderVisibility   visibility;
    };
    struct StaticSamplerDescription {
        StaticSamplerDescription(allocator_type alloc = {}) : name(alloc) {}
        std::pmr::string name;
        SamplerDesc      desc;
        unsigned         register_index;
        unsigned         space;
        ShaderVisibility visibility;
    };

    RootSignatureDetial(allocator_type alloc = {})
        : parameter_table(alloc), static_sampler_descs(alloc) {}

    std::pmr::vector<Parameter>                parameter_table;
    std::pmr::vector<StaticSamplerDescription> static_sampler_descs;
};

class RootSignature : public Resource, private RootSignatureDetial, public utils::enable_private_allocate_shared_build<RootSignature> {
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;

    class Builder : private RootSignatureDetial {
        friend class RootSignature;

    public:
        Builder(allocator_type alloc = {}) : RootSignatureDetial(alloc), allocator(alloc) {}
        Builder& Add(
            std::string_view   name,
            ShaderVariableType type,
            unsigned           register_index,
            unsigned           space,
            ShaderVisibility   visibility = ShaderVisibility::All);

        // TODO Sampler
        Builder& AddStaticSampler(
            std::string_view name,
            SamplerDesc      desc,
            unsigned         register_index,
            unsigned         space,
            ShaderVisibility visibility = ShaderVisibility::All);

        std::shared_ptr<RootSignature> Create(DriverAPI& driver);

    private:
        allocator_type allocator;
    };

    inline auto& GetParametes() const noexcept { return parameter_table; }
    inline auto& GetStaticSamplerDescs() const noexcept { return static_sampler_descs; }

protected:
    RootSignature(const Builder&, allocator_type);
};

struct PipelineStateDetial {
    using allocator_type = std::pmr::polymorphic_allocator<>;

    struct InputLayout {
        std::string_view      semantic_name;
        unsigned              semantic_index;
        Format                format;
        unsigned              input_slot;
        size_t                aligned_by_offset;
        std::optional<size_t> instance_count;
    };

    PipelineStateDetial(allocator_type alloc = {})
        : name(alloc),
          vertex_shader(alloc),
          pixel_shader(alloc),
          input_layout(alloc) {}

    std::pmr::string               name;
    core::Buffer                   vertex_shader;
    core::Buffer                   pixel_shader;
    std::pmr::vector<InputLayout>  input_layout;
    bool                           front_counter_clockwise = true;
    std::shared_ptr<RootSignature> root_signature          = nullptr;
    Format                         render_format           = Format::UNKNOWN;
    Format                         depth_buffer_format     = Format::UNKNOWN;
    resource::PrimitiveType        primitive_type          = resource::PrimitiveType::TriangleList;
    BlendDescription               blend_state;
    RasterizerDescription          rasterizer_state;
};

class PipelineState : public Resource, PipelineStateDetial, utils::enable_private_allocate_shared_build<PipelineState> {
public:
    using allocator_type = std::pmr::polymorphic_allocator<>;

    class Builder : private PipelineStateDetial {
        friend class PipelineState;

    public:
        Builder(allocator_type alloc = {}) : PipelineStateDetial(alloc), allocator(alloc) {}

        Builder& SetName(std::string_view name);
        Builder& SetVertexShader(core::Buffer vs);
        Builder& SetPixelShader(core::Buffer ps);
        template <std::size_t N>
        Builder& SetInputLayout(const std::array<InputLayout, N>& input_layout);
        Builder& SetRootSignautre(std::shared_ptr<RootSignature> sig);
        Builder& SetRenderFormat(Format format);
        Builder& SetDepthBufferFormat(Format format);
        Builder& SetPrimitiveType(resource::PrimitiveType type);
        Builder& SetRasterizerState(RasterizerDescription desc);
        Builder& SetBlendState(BlendDescription desc);

        std::shared_ptr<PipelineState> Build(DriverAPI& driver);

    private:
        allocator_type allocator;
    };

protected:
    PipelineState(const Builder& builder, allocator_type alloc);
};

template <std::size_t N>
auto PipelineState::Builder::SetInputLayout(const std::array<InputLayout, N>& data) -> Builder& {
    std::copy(data.begin(), data.end(), input_layout.begin());
    return *this;
}

}  // namespace hitagi::graphics