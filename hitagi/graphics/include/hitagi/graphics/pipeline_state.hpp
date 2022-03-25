#pragma once
#include "types.hpp"
#include "shader_manager.hpp"
#include "resource.hpp"

#include <string>
#include <optional>
#include <cassert>
#include <array>
#include <set>

namespace hitagi::graphics {
class DriverAPI;

struct InputLayout {
    std::string           semantic_name;
    unsigned              semantic_index;
    Format                format;
    unsigned              input_slot;
    size_t                aligned_by_offset;
    std::optional<size_t> instance_count;
};

class RootSignature : public Resource {
    struct Parameter {
        std::string        name;
        ShaderVariableType type;
        unsigned           register_index;
        unsigned           space;
        ShaderVisibility   visibility;
    };
    struct StaticSamplerDescription {
        std::string          name;
        Sampler::Description desc;
        unsigned             register_index;
        unsigned             space;
        ShaderVisibility     visibility;
    };

public:
    RootSignature(std::string_view name) : Resource(name, nullptr) {}
    RootSignature(const RootSignature&) = delete;
    RootSignature& operator=(const RootSignature&) = delete;
    RootSignature(RootSignature&&);
    RootSignature& operator=(RootSignature&&);

    RootSignature& Add(
        std::string_view   name,
        ShaderVariableType type,
        unsigned           register_index,
        unsigned           space,
        ShaderVisibility   visibility = ShaderVisibility::All);
    // TODO Sampler
    RootSignature& AddStaticSampler(
        std::string_view     name,
        Sampler::Description desc,
        unsigned             register_index,
        unsigned             sapce,
        ShaderVisibility     visibility = ShaderVisibility::All);
    RootSignature& Create(DriverAPI& driver);

    inline auto& GetParametes() const noexcept { return m_ParameterTable; }
    inline auto& GetStaticSamplerDescs() const noexcept { return m_StaticSamplerDescs; }

    operator bool() const noexcept { return m_Created; }

protected:
    bool                                  m_Created = false;
    std::vector<Parameter>                m_ParameterTable;
    std::vector<StaticSamplerDescription> m_StaticSamplerDescs;
};

class PipelineState : public Resource {
public:
    PipelineState(std::string_view name) : Resource(name, nullptr) {}

    PipelineState& SetVertexShader(std::shared_ptr<VertexShader> vs);
    PipelineState& SetPixelShader(std::shared_ptr<PixelShader> ps);
    PipelineState& SetInputLayout(const std::vector<InputLayout>& input_layout);
    PipelineState& SetRootSignautre(std::shared_ptr<RootSignature> sig);
    PipelineState& SetRenderFormat(Format format);
    PipelineState& SetDepthBufferFormat(Format format);
    PipelineState& SetPrimitiveType(PrimitiveType type);
    PipelineState& SetRasterizerState(RasterizerDescription desc);
    PipelineState& SetBlendState(BlendDescription desc);
    void           Create(DriverAPI& driver);

    inline const std::string&              GetName() const noexcept { return m_Name; }
    inline std::shared_ptr<VertexShader>   GetVS() const noexcept { return m_Vs; }
    inline std::shared_ptr<PixelShader>    GetPS() const noexcept { return m_Ps; }
    inline const std::vector<InputLayout>& GetInputLayout() const noexcept { return m_InputLayout; }
    inline std::shared_ptr<RootSignature>  GetRootSignature() const noexcept { return m_RootSignature; }
    inline Format                          GetRenderTargetFormat() const noexcept { return m_RenderFormat; }
    inline Format                          GetDepthBufferFormat() const noexcept { return m_DepthBufferFormat; }
    inline PrimitiveType                   GetPrimitiveType() const noexcept { return m_PrimitiveType; }
    inline BlendDescription                GetBlendState() const noexcept { return m_BlendState; }
    inline RasterizerDescription           GetRasterizerState() const noexcept { return m_RasterizerState; }
    inline bool                            IsFontCounterClockwise() const noexcept { return m_FrontCounterClockwise; }

private:
    bool                           m_Created = false;
    std::shared_ptr<VertexShader>  m_Vs;
    std::shared_ptr<PixelShader>   m_Ps;
    std::vector<InputLayout>       m_InputLayout;
    bool                           m_FrontCounterClockwise = true;
    std::shared_ptr<RootSignature> m_RootSignature         = nullptr;
    Format                         m_RenderFormat          = Format::UNKNOWN;
    Format                         m_DepthBufferFormat     = Format::UNKNOWN;
    PrimitiveType                  m_PrimitiveType         = PrimitiveType::TriangleList;
    BlendDescription               m_BlendState;
    RasterizerDescription          m_RasterizerState;
};

}  // namespace hitagi::graphics